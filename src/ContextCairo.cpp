#include <canvas/ContextCairo.h>

#include <algorithm>
#include <cassert>
#include <cmath>

using namespace canvas;
using namespace std;

#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 17, 2)
#define CANVAS_CAIRO_HAS_FLOAT_FORMAT 1
#endif

// Float colour: the largest straight (not premultiplied) component, in
// units of the numbers the client drew with.
static const float max_headroom = 18446744073709551616.0f;  // 2^64

// The smallest power of two, at least 1, that holds m. A power of two so
// that scaling by it is exact.
static float headroomFor(float m) {
  if (!(m > 1.0f)) return 1.0f;
  if (!std::isfinite(m) || m >= max_headroom) return max_headroom;
  int e;
  float f = std::frexp(m, &e);  // m = f * 2^e, f in [0.5, 1)
  return std::ldexp(1.0f, f == 0.5f ? e - 1 : e);
}

static float maxComponent(const Color & c) {
  float m = 0.0f;
  if (c.red > m) m = c.red;
  if (c.green > m) m = c.green;
  if (c.blue > m) m = c.blue;
  return m;
}

static float maxComponent(const Style & style) {
  float m = maxComponent(style.color);
  for (auto & stop : style.getColors()) m = std::max(m, maxComponent(stop.second));
  return m;
}

static float clamp01(float v) { return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f; }

// The pixel as premultiplied r, g, b, a floats, whatever the image's format.
// A mask is black with that alpha; bytes are v / 255 as they are (no transfer).
static void readPremultiplied(const ImageData & image, size_t i, float * out) {
  unsigned int channels = image.getNumChannels();
  bool bgr = image.getChannelOrder() == ChannelOrder::BGR;
  float v[4];
  if (image.isFloat()) {
    const float * p = image.getFloatData() + channels * i;
    for (unsigned int c = 0; c < channels; c++) v[c] = p[c];
  } else {
    const unsigned char * p = image.getData() + channels * i;
    for (unsigned int c = 0; c < channels; c++) v[c] = p[c] / 255.0f;
  }
  if (channels == 1) {
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = v[0];
    return;
  }
  out[0] = bgr ? v[2] : v[0];
  out[1] = v[1];
  out[2] = bgr ? v[0] : v[2];
  out[3] = 1.0f;
  if (channels == 4) {
    out[3] = v[3];
    if (!image.isPremultiplied()) {
      for (int c = 0; c < 3; c++) out[c] *= v[3];
    }
  }
}

// The largest straight colour component of a float image.
static float maxStraight(const ImageData & image) {
  if (!image.isFloat()) return 1.0f;
  float m = 0.0f, px[4];
  for (size_t i = 0; i < size_t(image.getWidth()) * image.getHeight(); i++) {
    readPremultiplied(image, i, px);
    float a = px[3];
    if (!(a > 0.0f)) continue;
    for (int c = 0; c < 3; c++) m = std::max(m, px[c] / a);
  }
  return m;
}

static cairo_format_t getCairoFormat(unsigned int num_channels, PixelType type = PixelType::UINT8) {
  if (type == PixelType::FLOAT32) {
#ifdef CANVAS_CAIRO_HAS_FLOAT_FORMAT
    return CAIRO_FORMAT_RGBA128F;
#else
    throw UnsupportedFormatException("this Cairo has no float image format (1.17.2 or later needed)");
#endif
  }
  switch (num_channels) {
  case 1: return CAIRO_FORMAT_A8;
  case 3: return CAIRO_FORMAT_RGB24;
  case 4: return CAIRO_FORMAT_ARGB32;
  default:
    // cerr << "unable to create Cairo format for " << num_channels << " channel(s)\n";
    assert(0);
    return CAIRO_FORMAT_ARGB32;
  }
}

struct CairoPixels {
  cairo_surface_t * surface;
  unsigned int * storage;
  float headroom;
};

// The surface's pixels are Cairo's: premultiplied 0xAARRGGBB words (A8 for
// one channel, with rows padded to Cairo's stride), or premultiplied RGBA
// floats divided by the headroom for a float surface. The image is converted
// from whatever format it says it is in; a float image on a byte surface is
// clamped to [0, 1]. A headroom of 0 is the smallest that holds a float image.
static CairoPixels initializeSurfaceFromData(const ImageData & image, PixelType type, float headroom) {
  unsigned int width = image.getWidth(), height = image.getHeight(), num_channels = image.getNumChannels();
  size_t numPixels = size_t(width) * height;
  if (type == PixelType::FLOAT32) {
    cairo_format_t format = getCairoFormat(4, type);
    if (headroom <= 0.0f) headroom = headroomFor(maxStraight(image));
    unsigned int stride = cairo_format_stride_for_width(format, width);
    unsigned int * storage = new unsigned int[(size_t(stride) * height + 3) / 4]();
    float scale = 1.0f / headroom;
    for (unsigned int y = 0; y < height; y++) {
      float * row = (float *)((unsigned char *)storage + size_t(y) * stride);
      for (unsigned int x = 0; x < width; x++) {
	float px[4];
	readPremultiplied(image, size_t(y) * width + x, px);
	row[4 * x + 0] = px[0] * scale;
	row[4 * x + 1] = px[1] * scale;
	row[4 * x + 2] = px[2] * scale;
	row[4 * x + 3] = px[3];
      }
    }
    auto surface = cairo_image_surface_create_for_data((unsigned char *)storage, format, width, height, stride);
    assert(surface);
    return { surface, storage, headroom };
  }

  cairo_format_t format = getCairoFormat(num_channels);
  unsigned int stride = cairo_format_stride_for_width(format, width);
  const unsigned char * data = image.getData();
  unsigned int * storage = new unsigned int[(size_t(stride) * height + 3) / 4]();
  if (image.isFloat()) {
    for (size_t i = 0; i < numPixels; i++) {
      float px[4];
      readPremultiplied(image, i, px);
      auto to_byte = [](float v) { return (unsigned int)(clamp01(v) * 255.0f + 0.5f); };
      if (num_channels == 1) {
	size_t y = i / width, x = i % width;
	((unsigned char *)storage)[y * stride + x] = (unsigned char)to_byte(px[3]);
      } else {
	// Colour channels cannot exceed alpha when premultiplied.
	unsigned int a = to_byte(px[3]), r = std::min(a, to_byte(px[0])), g = std::min(a, to_byte(px[1])), b = std::min(a, to_byte(px[2]));
	storage[i] = b | (g << 8) | (r << 16) | (a << 24);
      }
    }
  } else if (num_channels == 1) {
    for (unsigned int y = 0; y < height; y++) {
      memcpy((unsigned char *)storage + size_t(y) * stride, data + size_t(y) * width, width);
    }
  } else {
    assert(stride == 4 * width);
    bool bgr = image.getChannelOrder() == ChannelOrder::BGR;
    bool premultiplied = image.isPremultiplied();
    for (size_t i = 0; i < numPixels; i++) {
      const unsigned char * p = data + num_channels * i;
      unsigned int r = bgr ? p[2] : p[0], g = p[1], b = bgr ? p[0] : p[2], a = 255;
      if (num_channels == 4) {
	a = p[3];
	if (!premultiplied) {
	  r = (r * a + 127) / 255;
	  g = (g * a + 127) / 255;
	  b = (b * a + 127) / 255;
	}
      }
      storage[i] = b | (g << 8) | (r << 16) | (a << 24);
    }
  }
  auto surface = cairo_image_surface_create_for_data((unsigned char*)storage,
						     format,
						     width,
						     height,
						     stride);
  assert(surface);
  return { surface, storage, 1.0f };
}

bool
CairoSurface::isFloatSupported() {
  static const bool supported = [] {
#ifdef CANVAS_CAIRO_HAS_FLOAT_FORMAT
    // The library running may be older than the headers.
    auto s = cairo_image_surface_create(CAIRO_FORMAT_RGBA128F, 1, 1);
    bool ok = cairo_surface_status(s) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy(s);
    return ok;
#else
    return false;
#endif
  }();
  return supported;
}

cairo_format_t
CairoSurface::getFloatFormat() {
  return getCairoFormat(4, PixelType::FLOAT32);
}

// A float surface is always RGBA.
static unsigned int channelsFor(unsigned int num_channels, PixelType type) {
  return type == PixelType::FLOAT32 ? 4 : num_channels;
}

static void checkFloat(unsigned int num_channels, PixelType type) {
  if (type != PixelType::FLOAT32) return;
  if (!CairoSurface::isFloatSupported()) throw UnsupportedFormatException("this Cairo has no float image format (1.17.2 or later needed)");
  if (num_channels != 4) throw UnsupportedFormatException("float surfaces have 4 channels");
}

CairoSurface::CairoSurface(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels, PixelType _type)
  : Surface(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels, _type) {
  checkFloat(_num_channels, _type);
  if (_actual_width && _actual_height) {
    surface = cairo_image_surface_create(getCairoFormat(_num_channels, _type), _actual_width, _actual_height);
    assert(surface);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
      cairo_surface_destroy(surface);
      surface = 0;
      throw UnsupportedFormatException("Cairo could not create the surface");
    }
  } else {
    surface = 0;
  }
}

CairoSurface::CairoSurface(const ImageData & image)
  : CairoSurface(image, image.getPixelType(), 0.0f)
{
}

CairoSurface::CairoSurface(const ImageData & image, PixelType type, float _headroom)
  : Surface(image.getWidth(), image.getHeight(), image.getWidth(), image.getHeight(), channelsFor(image.getNumChannels(), type), type)
{
  checkFloat(4, type);
  auto p = initializeSurfaceFromData(image, type, _headroom);
  surface = p.surface;
  storage = p.storage;
  headroom = p.headroom;
}

CairoSurface::~CairoSurface() {
  if (cr) {
    cairo_destroy(cr);
  }
  if (surface) {
    cairo_surface_destroy(surface);
  }
  delete[] storage;
} 

void
CairoSurface::flush() {
  assert(surface);
  cairo_surface_flush(surface);
}

void
CairoSurface::markDirty() {
  assert(surface);
  cairo_surface_mark_dirty(surface);
}

void
CairoSurface::ensureHeadroom(float max_component) {
  if (!isFloat()) return;
  float wanted = headroomFor(max_component);
  if (wanted <= headroom) return;
  if (surface) {
    // Colour channels only: alpha is not scaled. Premultiplied blending is
    // linear in the colour channels, so the pixels keep their meaning.
    float factor = headroom / wanted;
    flush();
    unsigned char * base = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    int w = cairo_image_surface_get_width(surface), h = cairo_image_surface_get_height(surface);
    for (int y = 0; y < h; y++) {
      float * row = (float *)(base + size_t(y) * stride);
      for (int x = 0; x < w; x++) {
	row[4 * x + 0] *= factor;
	row[4 * x + 1] *= factor;
	row[4 * x + 2] *= factor;
      }
    }
    markDirty();
  }
  headroom = wanted;
}

// Cairo's solid colours are 16-bit and clamped, so a float surface paints a
// 1x1 float image (exact, and the colour is divided by the headroom).
void
CairoSurface::setSolidSource(const Color & color, float alpha) {
  if (!isFloat()) {
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, alpha);
    return;
  }
  float a = clamp01(alpha);
  auto one = cairo_image_surface_create(getFloatFormat(), 1, 1);
  float * px = (float *)cairo_image_surface_get_data(one);
  px[0] = color.red / headroom * a;
  px[1] = color.green / headroom * a;
  px[2] = color.blue / headroom * a;
  px[3] = a;
  cairo_surface_mark_dirty(one);
  cairo_set_source_surface(cr, one, 0, 0);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_surface_destroy(one);
}

void
CairoSurface::resize(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) {
  Surface::resize(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels);
  if (cr) {
    cairo_destroy(cr);
    cr = 0;
  }
  if (surface) cairo_surface_destroy(surface);  
  headroom = 1.0f;
  surface = cairo_image_surface_create(getCairoFormat(_num_channels, getPixelType()), _actual_width, _actual_height);
  assert(surface);
} 

void
CairoSurface::sendPath(const Path2D & path, float displayScale) {
  initializeContext();

  cairo_new_path(cr);
  for (auto pc : path.getData()) {
    switch (pc.type) {
    case PathComponent::MOVE_TO:
      cairo_move_to(cr, displayScale * pc.x0, displayScale * pc.y0);
      break;
    case PathComponent::LINE_TO:
      cairo_line_to(cr, displayScale * pc.x0, displayScale * pc.y0);
      break;
    case PathComponent::CLOSE: cairo_close_path(cr); break;
    case PathComponent::ARC:
      if (!pc.anticlockwise) {
	cairo_arc(cr, displayScale * pc.x0, displayScale * pc.y0, displayScale * pc.radius, pc.sa, pc.ea);
      } else {
	cairo_arc_negative(cr, displayScale * pc.x0, displayScale * pc.y0, displayScale * pc.radius, pc.sa, pc.ea);
      }
      break;
    }
  }
}

void
CairoSurface::renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, const LineStyle & lineStyle) {
  initializeContext();
  ensureHeadroom(maxComponent(style));
  // Colours go to Cairo divided by the headroom (1 on byte surfaces).
  auto stop = [&](const Color & c) { return Color(c.red / headroom, c.green / headroom, c.blue / headroom, c.alpha * globalAlpha); };

  if (!clipPath.empty()) {
    sendPath(clipPath, displayScale);
    cairo_clip(cr);
  }

  if (mode == RenderMode::STROKE) {
    if (!lineStyle.dash.empty()) {
      std::vector<double> scaled_dash;
      for (auto & v : lineStyle.dash) scaled_dash.push_back(transformation.transformSize(v) * displayScale);
      cairo_set_dash(cr, scaled_dash.data(), scaled_dash.size(), transformation.transformSize(lineStyle.dashOffset) * displayScale);
    } else {
      cairo_set_dash(cr, 0, 0, 0);
    }
    switch (lineStyle.cap) {
    case LineCap::BUTT: cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT); break;
    case LineCap::ROUND: cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); break;
    case LineCap::SQUARE: cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE); break;
    }
    switch (lineStyle.join) {
    case LineJoin::MITER: cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER); break;
    case LineJoin::ROUND: cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); break;
    case LineJoin::BEVEL: cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); break;
    }
    cairo_set_miter_limit(cr, lineStyle.miterLimit);
  }
  
  switch (op) {
  case Operator::SOURCE_OVER: cairo_set_operator(cr, CAIRO_OPERATOR_OVER); break;
  case Operator::COPY: cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); break;
  }
  
  cairo_pattern_t * pat = 0;
  if (style.getType() == Style::LINEAR_GRADIENT) {
    pat = cairo_pattern_create_linear(style.p0.x * displayScale, style.p0.y * displayScale,
				      style.p1.x * displayScale, style.p1.y * displayScale);
    for (auto it = style.getColors().begin(); it != style.getColors().end(); it++) {
      auto c = stop(it->second);
      cairo_pattern_add_color_stop_rgba(pat, it->first, c.red, c.green, c.blue, c.alpha);
    }
    cairo_set_source(cr, pat);
  } else if (style.getType() == Style::RADIAL_GRADIENT) {
    pat = cairo_pattern_create_radial(style.p0.x * displayScale, style.p0.y * displayScale, style.r0 * displayScale,
				      style.p1.x * displayScale, style.p1.y * displayScale, style.r1 * displayScale);
    for (auto it = style.getColors().begin(); it != style.getColors().end(); it++) {
      auto c = stop(it->second);
      cairo_pattern_add_color_stop_rgba(pat, it->first, c.red, c.green, c.blue, c.alpha);
    }
    cairo_set_source(cr, pat);
  } else {
    // A filter style has no filters yet: the style's solid colour, never the previous source.
    setSolidSource(style.color, style.color.alpha * globalAlpha);
  }
  sendPath(path, displayScale);
  switch (mode) {
  case RenderMode::STROKE:
    cairo_set_line_width(cr, lineWidth * displayScale);
    // cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);  
    break;
  case RenderMode::FILL:
    cairo_fill(cr);
    break;
  }
  
  if (pat) {
    cairo_pattern_destroy(pat);
  }
  if (pat || isFloat()) {
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  }
  if (!clipPath.empty()) {
    cairo_reset_clip(cr);
  }
}

void
CairoSurface::renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float displayScale, float alpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) {
  initializeContext();
  ensureHeadroom(maxComponent(style.color));

  if (!clipPath.empty()) {
    sendPath(clipPath, displayScale);
    cairo_clip(cr);
  }

  switch (op) {
  case Operator::SOURCE_OVER: cairo_set_operator(cr, CAIRO_OPERATOR_OVER); break;
  case Operator::COPY: cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); break;
  }

  auto actual_font_size = font.size * displayScale;
  bool has_hinting = (font.hinting || font.cleartype) && actual_font_size <= 24;
  if (has_hinting) {
    auto options = cairo_font_options_create();
    if (font.cleartype) {
      cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
    } else {
      cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    }
    if (font.cleartype || font.hinting) {
      cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_FULL);
      cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_ON);
    } else {
      cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
      cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
    }
    cairo_set_font_options(cr, options);
    cairo_font_options_destroy(options);
  }
  
  setSolidSource(style.color, style.color.alpha * alpha);
  cairo_select_font_face(cr, font.family.c_str(),
			 font.style == Font::NORMAL_STYLE ? CAIRO_FONT_SLANT_NORMAL : (font.style == Font::ITALIC ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_OBLIQUE),
			 font.weight.isBold() ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, actual_font_size);
  
  auto x = p.x * displayScale, y = p.y * displayScale;

  if (textBaseline == TextBaseline::MIDDLE || textBaseline == TextBaseline::TOP) {
    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    
    switch (textBaseline) {
      // case TextBaseline::MIDDLE: y -= (extents.height/2 + extents.y_bearing); break;
    case TextBaseline::MIDDLE: y += -font_extents.descent + (font_extents.ascent + font_extents.descent) / 2.0; break;
    case TextBaseline::TOP: y += font_extents.ascent; break;
    default: break;
    }
  }

  if (textAlign != TextAlign::LEFT) {
    cairo_text_extents_t text_extents;
    cairo_text_extents(cr, text.c_str(), &text_extents);
    
    switch (textAlign) {
    case TextAlign::LEFT: break;
    case TextAlign::CENTER: x -= text_extents.width / 2; break;
    case TextAlign::RIGHT: x -= text_extents.width; break;
    default: break;
    }
  }
  
  cairo_move_to(cr, x, y);
  
  switch (mode) {
  case RenderMode::STROKE:
    cairo_set_line_width(cr, lineWidth * displayScale);
    cairo_text_path(cr, text.c_str());
    cairo_stroke(cr);
    break;
  case RenderMode::FILL:
    if (has_hinting) {
      cairo_show_text(cr, text.c_str());
    } else {
      cairo_text_path(cr, text.c_str());
      cairo_fill(cr);
    }
    break;
  }

  if (isFloat()) {
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  }
  if (!clipPath.empty()) {
    cairo_reset_clip(cr);
  }
}

TextMetrics
CairoSurface::measureText(const Font & font, const std::string & text, TextBaseline textBaseline, float displayScale) {
  initializeContext();
  cairo_select_font_face(cr, font.family.c_str(), font.style == Font::NORMAL_STYLE ? CAIRO_FONT_SLANT_NORMAL : (font.style == Font::ITALIC ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_OBLIQUE), font.weight.isBold() ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, font.size * displayScale);
  cairo_text_extents_t te;
  cairo_text_extents(cr, text.c_str(), &te);

  cairo_font_extents_t fe;
  cairo_font_extents(cr, &fe);

  int baseline = 0;
  if (textBaseline == TextBaseline::MIDDLE) {
    baseline = (fe.ascent + fe.descent) / 2;
  } else if (textBaseline == TextBaseline::TOP) {
    baseline = (fe.ascent + fe.descent);
  }
  return TextMetrics((float) te.width / displayScale, (fe.descent - baseline) / displayScale, (fe.ascent - baseline) / displayScale); //, (float)te.height);
}

void
CairoSurface::drawNativeSurface(CairoSurface & img, const Point & p, double w, double h, float displayScale, float globalAlpha, const Path2D & clipPath, bool imageSmoothingEnabled) {
  initializeContext();

  if (!clipPath.empty()) {
    sendPath(clipPath, displayScale);
    cairo_clip(cr);
  }

  double px = p.x * displayScale, py = p.y * displayScale;
  double sx = w * displayScale / img.getActualWidth(), sy = h * displayScale / img.getActualHeight();
  cairo_save(cr);
  cairo_scale(cr, sx, sy);
  cairo_set_source_surface(cr, img.surface, (px / sx), (py / sy));
  // BEST is a fixed-point convolution on float images and loses the dark
  // range and the edges (a value of 1e-4 comes back halved); GOOD is exact.
  cairo_pattern_set_filter(cairo_get_source(cr), imageSmoothingEnabled ? (isFloat() ? CAIRO_FILTER_GOOD : CAIRO_FILTER_BEST) : CAIRO_FILTER_NEAREST);
  if (globalAlpha < 1.0f) {
    cairo_paint_with_alpha(cr, globalAlpha);
  } else {
    cairo_paint(cr);
  }
  cairo_set_source_rgb(cr, 0.0f, 0.0f, 0.0f); // is this needed?
  cairo_restore(cr);

  if (!clipPath.empty()) {
    cairo_reset_clip(cr);
  }
}

void
CairoSurface::drawImage(Surface & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled) {
  CairoSurface * cs_ptr = dynamic_cast<CairoSurface*>(&_img);
  if (cs_ptr && cs_ptr->isFloat() == isFloat() && (!isFloat() || cs_ptr->headroom == headroom)) {
    drawNativeSurface(*cs_ptr, p, w, h, displayScale, globalAlpha, clipPath, imageSmoothingEnabled);    
  } else {
    // Another backend's surface, or one whose pixels are not in this
    // surface's units: go through ImageData.
    auto img = _img.createImage(displayScale);
    drawImage(img->getData(), p, w, h, displayScale, globalAlpha, shadowBlur, shadowOffsetX, shadowOffsetY, shadowColor, clipPath, imageSmoothingEnabled);
  }
}

void
CairoSurface::drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled) {
  if (isFloat()) {
    // The image in this surface's units; bytes are v / 255 as they are.
    initializeContext();
    ensureHeadroom(maxStraight(_img));
    CairoSurface img(_img, PixelType::FLOAT32, headroom);
    drawNativeSurface(img, p, w, h, displayScale, globalAlpha, clipPath, imageSmoothingEnabled);
  } else {
    // Bytes; a float image is clamped to [0, 1] on the way.
    CairoSurface img(_img, PixelType::UINT8, 0.0f);
    drawNativeSurface(img, p, w, h, displayScale, globalAlpha, clipPath, imageSmoothingEnabled);
  }
}

class CairoImage : public Image {
public:
  void setConverter(FilenameConverter * _converter) { converter = _converter; }

  CairoImage(float _display_scale) : Image(_display_scale) { }
  CairoImage(const std::string & filename, float _display_scale) : Image(filename, _display_scale) { }
  CairoImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels, float _display_scale) : Image(_data, _width, _height, _num_channels, _display_scale) { }
  
protected:
  void loadFile() override {
    std::string path = filename;
    if (converter && !converter->convert(filename, path)) path = filename;
    data = loadFromFile(path);
    if (!data.get()) filename.clear();
  }

private:
  FilenameConverter * converter = nullptr;
};


std::unique_ptr<Image>
CairoContextFactory::loadImage(const std::string & filename) {
  auto image = std::make_unique<CairoImage>(filename, getDisplayScale());
  image->setConverter(converter);
  return image;
}

std::unique_ptr<Image>
CairoContextFactory::createImage() {
  return std::make_unique<CairoImage>(getDisplayScale());
}

std::unique_ptr<Image>
CairoContextFactory::createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) {
  return std::make_unique<CairoImage>(_data, _width, _height, _num_channels, getDisplayScale());
}

// Cairo's RGB24 is a padded 32-bit word per pixel; ImageData's 3 channels are not.
std::unique_ptr<ImageData>
CairoSurface::readPixels() {
  if (isFloat()) {
    // Cairo's float pixels are premultiplied r, g, b, a: RGB order. The
    // colour channels are multiplied back to the numbers that were drawn.
    unsigned int w = getActualWidth(), h = getActualHeight();
    auto r = std::make_unique<ImageData>(w, h, 4, PixelType::FLOAT32);
    r->setFormat(ChannelOrder::RGB, true);
    if (surface) {
      auto src = (const unsigned char *)lockMemory(false);
      size_t stride = getMemoryStride();
      for (unsigned int y = 0; y < h; y++) {
	const float * in = (const float *)(src + y * stride);
	float * out = r->getFloatData() + size_t(y) * w * 4;
	for (unsigned int x = 0; x < w; x++) {
	  out[4 * x + 0] = in[4 * x + 0] * headroom;
	  out[4 * x + 1] = in[4 * x + 1] * headroom;
	  out[4 * x + 2] = in[4 * x + 2] * headroom;
	  out[4 * x + 3] = in[4 * x + 3];
	}
      }
      releaseMemory();
    }
    return r;
  }
  if (getNumChannels() != 3) return Surface::readPixels();
  unsigned int w = getActualWidth(), h = getActualHeight();
  auto r = std::make_unique<ImageData>(w, h, 3);
  r->setFormat(ChannelOrder::BGR, false);
  auto src = (const unsigned char *)lockMemory(false);
  if (src) {
    size_t stride = getMemoryStride();
    for (unsigned int y = 0; y < h; y++) {
      for (unsigned int x = 0; x < w; x++) {
	memcpy(r->getData() + (size_t(y) * w + x) * 3, src + y * stride + 4 * x, 3);
      }
    }
  }
  releaseMemory();
  return r;
}

std::unique_ptr<Image>
CairoSurface::createImage(float display_scale) {
  auto image = std::make_unique<CairoImage>(display_scale);
  image->getDataPtr() = readPixels();
  return image;
}
