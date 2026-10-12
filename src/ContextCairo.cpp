#include <canvas/ContextCairo.h>

#include <cassert>
#include <cmath>

using namespace canvas;
using namespace std;

static cairo_format_t getCairoFormat(unsigned int num_channels) {
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

// The surface's pixels are Cairo's: premultiplied 0xAARRGGBB words (A8 for
// one channel, with rows padded to Cairo's stride). The image is converted
// from whatever format it says it is in.
static pair<cairo_surface_t *, unsigned int *> initializeSurfaceFromData(const ImageData & image) {
  unsigned int width = image.getWidth(), height = image.getHeight(), num_channels = image.getNumChannels();
  cairo_format_t format = getCairoFormat(num_channels);
  unsigned int stride = cairo_format_stride_for_width(format, width);
  const unsigned char * data = image.getData();
  size_t numPixels = size_t(width) * height;
  unsigned int * storage = new unsigned int[(size_t(stride) * height + 3) / 4]();
  if (num_channels == 1) {
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
  return std::pair<cairo_surface_t *, unsigned int *>(surface, storage);
}

CairoSurface::CairoSurface(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels)
  : Surface(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels) {
  if (_actual_width && _actual_height) {
    surface = cairo_image_surface_create(getCairoFormat(_num_channels), _actual_width, _actual_height);
    assert(surface);
  } else {
    surface = 0;
  }
}

CairoSurface::CairoSurface(const ImageData & image)
  : Surface(image.getWidth(), image.getHeight(), image.getWidth(), image.getHeight(), image.getNumChannels())
{
  auto p = initializeSurfaceFromData(image);
  surface = p.first;
  storage = p.second;
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
CairoSurface::resize(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) {
  Surface::resize(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels);
  if (cr) {
    cairo_destroy(cr);
    cr = 0;
  }
  if (surface) cairo_surface_destroy(surface);  
  surface = cairo_image_surface_create(getCairoFormat(_num_channels), _actual_width, _actual_height);
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
      cairo_pattern_add_color_stop_rgba(pat, it->first, it->second.red, it->second.green, it->second.blue, it->second.alpha * globalAlpha);
    }
    cairo_set_source(cr, pat);
  } else if (style.getType() == Style::RADIAL_GRADIENT) {
    pat = cairo_pattern_create_radial(style.p0.x * displayScale, style.p0.y * displayScale, style.r0 * displayScale,
				      style.p1.x * displayScale, style.p1.y * displayScale, style.r1 * displayScale);
    for (auto it = style.getColors().begin(); it != style.getColors().end(); it++) {
      cairo_pattern_add_color_stop_rgba(pat, it->first, it->second.red, it->second.green, it->second.blue, it->second.alpha * globalAlpha);
    }
    cairo_set_source(cr, pat);
  } else if (style.getType() == Style::FILTER) {
    // No filters yet: the style's solid colour, never the previous source.
    cairo_set_source_rgba(cr, style.color.red, style.color.green, style.color.blue, style.color.alpha * globalAlpha);
  } else {
    cairo_set_source_rgba(cr, style.color.red, style.color.green, style.color.blue, style.color.alpha * globalAlpha);
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
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  }
  if (!clipPath.empty()) {
    cairo_reset_clip(cr);
  }
}

void
CairoSurface::renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float displayScale, float alpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) {
  initializeContext();

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
  
  cairo_set_source_rgba(cr, style.color.red, style.color.green, style.color.blue, style.color.alpha * alpha);
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
  cairo_pattern_set_filter(cairo_get_source(cr), imageSmoothingEnabled ? CAIRO_FILTER_BEST : CAIRO_FILTER_NEAREST);
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
  if (cs_ptr) {
    drawNativeSurface(*cs_ptr, p, w, h, displayScale, globalAlpha, clipPath, imageSmoothingEnabled);    
  } else {
    auto img = _img.createImage(displayScale);
    CairoSurface cs(img->getData());
    drawNativeSurface(cs, p, w, h, displayScale, globalAlpha, clipPath, imageSmoothingEnabled);
  }
}

void
CairoSurface::drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled) {
  CairoSurface img(_img);
  drawNativeSurface(img, p, w, h, displayScale, globalAlpha, clipPath, imageSmoothingEnabled);
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
