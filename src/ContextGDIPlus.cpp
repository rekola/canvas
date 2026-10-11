#include <ContextGDIPlus.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>

bool canvas::ContextGDIPlus::is_initialized = false;
ULONG_PTR canvas::ContextGDIPlus::m_gdiplusToken;

using namespace std;
using namespace canvas;

static inline wstring from_utf8(const std::string& s) {
  std::unique_ptr<wchar_t[]> tmp(new wchar_t[s.size() + 1]);
  auto r = MultiByteToWideChar(CP_UTF8, 0, s.data(), s.size(), tmp.get(), s.size() + 1);
  return wstring(tmp.get(), static_cast<size_t>(r));
}

static void toGDIPath(const Path2D & path, Gdiplus::GraphicsPath & output, float display_scale) {  
  output.StartFigure();
  Gdiplus::PointF current_pos;

  for (auto pc : path.getData()) {
    switch (pc.type) {
    case PathComponent::MOVE_TO:
      current_pos = Gdiplus::PointF(Gdiplus::REAL(pc.x0 * display_scale), Gdiplus::REAL(pc.y0 * display_scale));
      break;
    case PathComponent::LINE_TO:
      {
	Gdiplus::PointF point(Gdiplus::REAL(pc.x0 * display_scale), Gdiplus::REAL(pc.y0 * display_scale));
	output.AddLine(current_pos, point);
	current_pos = point;
      }
      break;
    case PathComponent::CLOSE:
      output.CloseFigure();
      break;
    case PathComponent::ARC:
      {
	double span = 0;
	if (0 && ((!pc.anticlockwise && (pc.ea - pc.sa >= 2 * M_PI)) || (pc.anticlockwise && (pc.sa - pc.ea >= 2 * M_PI)))) {
	  // If the anticlockwise argument is false and endAngle-startAngle is equal to or greater than 2*PI, or, if the
	  // anticlockwise argument is true and startAngle-endAngle is equal to or greater than 2*PI, then the arc is the whole
	  // circumference of this circle.
	  span = 2 * M_PI;
	} else {
	  if (!pc.anticlockwise && (pc.ea < pc.sa)) {
	    span += 2 * M_PI;
	  } else if (pc.anticlockwise && (pc.sa < pc.ea)) {
	    span -= 2 * M_PI;
	  }
 
#if 0
    // this is also due to switched coordinate system
    // we would end up with a 0 span instead of 360
    if (!(qFuzzyCompare(span + (ea - sa) + 1, 1.0) && qFuzzyCompare(qAbs(span), 360.0))) {
      // mod 360
      span += (ea - sa) - (static_cast<int>((ea - sa) / 360)) * 360;
    }
#else
	  span += pc.ea - pc.sa;
#endif
	}
 
#if 0
  // If the path is empty, move to where the arc will start to avoid painting a line from (0,0)
  // NOTE: QPainterPath::isEmpty() won't work here since it ignores a lone MoveToElement
  if (!m_path.elementCount())
    m_path.arcMoveTo(xs, ys, width, height, sa);
  else if (!radius) {
    m_path.lineTo(xc, yc);
    return;
  }
#endif

#if 0
  if (anticlockwise) {
    span = -M_PI / 2.0;
  } else {
    span = M_PI / 2.0;
  }
#endif
  Gdiplus::RectF rect(Gdiplus::REAL(pc.x0 * display_scale - pc.radius * display_scale), Gdiplus::REAL(pc.y0 * display_scale - pc.radius * display_scale), Gdiplus::REAL(2 * pc.radius * display_scale), Gdiplus::REAL(2 * pc.radius * display_scale));

	output.AddArc(rect, Gdiplus::REAL(pc.sa * 180.0f / M_PI), Gdiplus::REAL(span * 180.0f / M_PI));
	output.GetLastPoint(&current_pos);
      }
      break;
    }
  }
}

static Gdiplus::Color toGDIColor(const Color & input, float globalAlpha = 1.0f) {
  int red = int(input.red * 255), green = int(input.green * 255), blue = int(input.blue * 255), alpha = int(input.alpha * globalAlpha * 255);
  if (red < 0) red = 0;
  else if (red > 255) red = 255;
  if (green < 0) green = 0;
  else if (green > 255) green = 255;
  if (blue < 0) blue = 0;
  else if (blue > 255) blue = 255;
  if (alpha < 0) alpha = 0;
  else if (alpha > 255) alpha = 255;
#if 0
  return Gdiplus::Color::FromArgb(alpha, red, green, blue);
#else
  return Gdiplus::Color(alpha, red, green, blue);
#endif
}

// A brush for a fill or a stroke. A gradient is padded beyond its end
// points with the end colours, as in HTML5, instead of tiling: the
// gradient line is extended past the path's extents and the stops are
// rescaled onto it.
static std::unique_ptr<Gdiplus::Brush> createBrush(const Style & style, float globalAlpha, float display_scale, const Path2D & path, std::unique_ptr<Gdiplus::Brush> * backdrop = nullptr) {
  const auto & stops = style.getColors();
  if (style.getType() == Style::RADIAL_GRADIENT && !stops.empty()) {
    // GDI+ has no two-circle gradient. A path gradient over the outer circle
    // with its centre at the inner one is exact for concentric circles and
    // an approximation otherwise. It paints nothing outside the circle, so
    // the caller fills that with the last colour first (the backdrop).
    Color first = stops.begin()->second, last = stops.rbegin()->second;
    double r0 = style.r0 * display_scale, r1 = style.r1 * display_scale;
    if (stops.size() == 1 || r1 <= 0.0 || r1 <= r0) {
      return std::make_unique<Gdiplus::SolidBrush>(toGDIColor(last, globalAlpha));
    }
    Gdiplus::GraphicsPath circle;
    double cx = style.p1.x * display_scale, cy = style.p1.y * display_scale;
    circle.AddEllipse(Gdiplus::RectF(Gdiplus::REAL(cx - r1), Gdiplus::REAL(cy - r1), Gdiplus::REAL(2 * r1), Gdiplus::REAL(2 * r1)));
    auto brush = std::make_unique<Gdiplus::PathGradientBrush>(&circle);
    brush->SetCenterPoint(Gdiplus::PointF(Gdiplus::REAL(style.p0.x * display_scale), Gdiplus::REAL(style.p0.y * display_scale)));
    // Positions run from 0 at the circle to 1 at the centre.
    std::vector<Gdiplus::Color> colors;
    std::vector<Gdiplus::REAL> positions;
    colors.push_back(toGDIColor(last, globalAlpha));
    positions.push_back(0.0f);
    for (auto it = stops.rbegin(); it != stops.rend(); ++it) {
      double t = std::min(1.0, std::max(0.0, double(it->first)));
      double u = (r0 + t * (r1 - r0)) / r1;  // distance from the centre as a fraction of r1
      double pos = std::min(1.0, std::max(0.0, 1.0 - u));
      if (pos <= positions.back()) pos = positions.back();
      colors.push_back(toGDIColor(it->second, globalAlpha));
      positions.push_back(Gdiplus::REAL(pos));
    }
    colors.push_back(toGDIColor(first, globalAlpha));
    positions.push_back(1.0f);
    brush->SetInterpolationColors(colors.data(), positions.data(), (INT)colors.size());
    if (backdrop) *backdrop = std::make_unique<Gdiplus::SolidBrush>(toGDIColor(last, globalAlpha));
    return brush;
  }
  if (style.getType() != Style::LINEAR_GRADIENT || stops.empty()) {
    return std::make_unique<Gdiplus::SolidBrush>(toGDIColor(style.color, globalAlpha));
  }
  Color first = stops.begin()->second, last = stops.rbegin()->second;
  double x0 = style.p0.x * display_scale, y0 = style.p0.y * display_scale;
  double dx = style.p1.x * display_scale - x0, dy = style.p1.y * display_scale - y0;
  double len2 = dx * dx + dy * dy;
  if (stops.size() == 1 || len2 == 0.0) {
    return std::make_unique<Gdiplus::SolidBrush>(toGDIColor(last, globalAlpha));
  }
  double min_x, min_y, max_x, max_y;
  path.getExtents(min_x, min_y, max_x, max_y);
  double xs[2] = { min_x * display_scale, max_x * display_scale }, ys[2] = { min_y * display_scale, max_y * display_scale };
  double umin = 0.0, umax = 1.0;
  for (double x : xs) {
    for (double y : ys) {
      double u = ((x - x0) * dx + (y - y0) * dy) / len2;
      umin = std::min(umin, u);
      umax = std::max(umax, u);
    }
  }
  double k = std::max(-umin, umax - 1.0) + 0.01;
  auto brush = std::make_unique<Gdiplus::LinearGradientBrush>(
    Gdiplus::PointF(Gdiplus::REAL(x0 - dx * k), Gdiplus::REAL(y0 - dy * k)),
    Gdiplus::PointF(Gdiplus::REAL(x0 + dx * (1.0 + k)), Gdiplus::REAL(y0 + dy * (1.0 + k))),
    toGDIColor(first, globalAlpha), toGDIColor(last, globalAlpha));
  std::vector<Gdiplus::Color> colors;
  std::vector<Gdiplus::REAL> positions;
  colors.push_back(toGDIColor(first, globalAlpha));
  positions.push_back(0.0f);
  for (auto & stop : stops) {
    double t = std::min(1.0, std::max(0.0, double(stop.first)));
    colors.push_back(toGDIColor(stop.second, globalAlpha));
    positions.push_back(Gdiplus::REAL((t + k) / (1.0 + 2.0 * k)));
  }
  colors.push_back(toGDIColor(last, globalAlpha));
  positions.push_back(1.0f);
  brush->SetInterpolationColors(colors.data(), positions.data(), (INT)colors.size());
  return brush;
}

GDIPlusSurface::GDIPlusSurface(const std::string & filename) : Surface(0, 0, 0, 0, false) {
  auto tmp = from_utf8(filename);
  bitmap = std::unique_ptr<Gdiplus::Bitmap>(Gdiplus::Bitmap::FromFile(tmp.data()));
  Surface::resize(bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetWidth(), bitmap->GetHeight(), true);
}

GDIPlusSurface::GDIPlusSurface(const unsigned char * buffer, size_t size) : Surface(0, 0, 0, 0, false) {
	assert(0);
}


void
GDIPlusSurface::renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & input_path, const Style & style, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color& shadowColor, const Path2D& clipPath, const LineStyle & lineStyle) {
  initializeContext();
  
  if (!clipPath.empty()) {
    Gdiplus::GraphicsPath path;
    toGDIPath(clipPath, path, display_scale);

    Gdiplus::Region region(&path);
    g->SetClip(&region);
  }

  switch (op) {
  case Operator::SOURCE_OVER:
    g->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    break;
  case Operator::COPY:
    g->SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    break;    
  }

  Gdiplus::GraphicsPath path;
  toGDIPath(input_path, path, display_scale);

  std::unique_ptr<Gdiplus::Brush> backdrop;
  auto brush = createBrush(style, globalAlpha, display_scale, input_path, &backdrop);
  switch (mode) {
  case RenderMode::STROKE:
    {
      float width = lineWidth * display_scale;
      Gdiplus::Pen pen(brush.get(), width);
      Gdiplus::LineCap cap = Gdiplus::LineCapFlat;
      Gdiplus::DashCap dash_cap = Gdiplus::DashCapFlat;
      if (lineStyle.cap == LineCap::ROUND) {
	cap = Gdiplus::LineCapRound;
	dash_cap = Gdiplus::DashCapRound;
      } else if (lineStyle.cap == LineCap::SQUARE) {
	cap = Gdiplus::LineCapSquare;
      }
      pen.SetLineCap(cap, cap, dash_cap);
      switch (lineStyle.join) {
      case LineJoin::MITER: pen.SetLineJoin(Gdiplus::LineJoinMiter); break;
      case LineJoin::ROUND: pen.SetLineJoin(Gdiplus::LineJoinRound); break;
      case LineJoin::BEVEL: pen.SetLineJoin(Gdiplus::LineJoinBevel); break;
      }
      pen.SetMiterLimit(std::max(1.0f, lineStyle.miterLimit));
      if (!lineStyle.dash.empty() && width > 0.0f) {
	// GDI+ measures dashes in pen widths.
	std::vector<float> dash;
	for (auto & v : lineStyle.dash) dash.push_back(std::max(0.001f, transformation.transformSize(v) * display_scale / width));
	pen.SetDashStyle(Gdiplus::DashStyleCustom);
	pen.SetDashPattern(dash.data(), (INT)dash.size());
	pen.SetDashOffset(transformation.transformSize(lineStyle.dashOffset) * display_scale / width);
      }
      g->DrawPath(&pen, &path);
    }
    break;
  case RenderMode::FILL:
    if (backdrop) {
      // Outside the gradient's outer circle, which the path gradient leaves unpainted.
      Gdiplus::GraphicsPath circle;
      double r1 = style.r1 * display_scale;
      circle.AddEllipse(Gdiplus::RectF(Gdiplus::REAL(style.p1.x * display_scale - r1), Gdiplus::REAL(style.p1.y * display_scale - r1), Gdiplus::REAL(2 * r1), Gdiplus::REAL(2 * r1)));
      Gdiplus::Region outside(&path);
      outside.Exclude(&circle);
      g->FillRegion(backdrop.get(), &outside);
    }
    g->FillPath(brush.get(), &path);
    break;
  }

  if (!clipPath.empty()) {
    g->ResetClip();
  }
}

void
GDIPlusSurface::drawNativeSurface(GDIPlusSurface & img, const Point & p, double w, double h, float displayScale, float globalAlpha, bool imageSmoothingEnabled) {
  if (!initializeContext()) return;

  g->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
  
  if (imageSmoothingEnabled) {
    // g->SetInterpolationMode( Gdiplus::InterpolationModeHighQualityBicubic );
    g->SetInterpolationMode( Gdiplus::InterpolationModeHighQualityBilinear );
  } else {
    g->SetInterpolationMode( Gdiplus::InterpolationModeNearestNeighbor );
  }
  // The colour matrix scales alpha by globalAlpha. Flipping at the edges
  // keeps the interpolation from fading the image's border.
  Gdiplus::ImageAttributes attributes;
  Gdiplus::ColorMatrix color_matrix = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, globalAlpha, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
  attributes.SetColorMatrix(&color_matrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
  attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
  Gdiplus::RectF dest(Gdiplus::REAL(p.x * displayScale), Gdiplus::REAL(p.y * displayScale), Gdiplus::REAL(w * displayScale), Gdiplus::REAL(h * displayScale));
  g->DrawImage(img.bitmap.get(), dest, 0.0f, 0.0f, Gdiplus::REAL(img.getActualWidth()), Gdiplus::REAL(img.getActualHeight()), Gdiplus::UnitPixel, &attributes);
}

void
GDIPlusSurface::configureFonts(const Font & font, float displayScale, Gdiplus::StringFormat * format) {
  auto scaled_font_size = font.size * displayScale;

  if (font.cleartype && scaled_font_size >= 2.0f && scaled_font_size <= 16.0f) {
    g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  } else if (font.hinting && scaled_font_size >= 2.0f && scaled_font_size <= 20.0f) {
    g->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
  } else {
    g->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);  
  }

  format->SetFormatFlags(Gdiplus::StringFormatFlagsMeasureTrailingSpaces |
	  		 Gdiplus::StringFormatFlagsNoWrap |
			 Gdiplus::StringFormatFlagsNoClip |
			 Gdiplus::StringFormatFlagsLineLimit |
			 Gdiplus::StringFormatFlagsDisplayFormatControl |
			 4 // FitBlackBox
    
		   // | Gdiplus::StringFormatFlagsNoFitBlackBox |
		   // | Gdiplus::StringFormatFlagsNoFontFallback
		   // | Gdiplus::StringFormatFlagsBypassGDI
		   );
  format->SetHotkeyPrefix(Gdiplus::HotkeyPrefixNone);
  format->SetTrimming(Gdiplus::StringTrimmingNone);
  format->SetAlignment(Gdiplus::StringAlignmentNear);
  format->SetLineAlignment(Gdiplus::StringAlignmentNear);
}

void
GDIPlusSurface::renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color& shadowColor, const Path2D& clipPath) {
  if (!initializeContext()) return;

  if (!clipPath.empty()) {
    Gdiplus::GraphicsPath path;
    toGDIPath(clipPath, path, display_scale);

    Gdiplus::Region region(&path);
    g->SetClip(&region);
  }

  float x = roundf(p.x * display_scale), y = roundf(p.y * display_scale);
  auto scaled_font_size = font.size * display_scale;

  Gdiplus::StringFormat f;
  configureFonts(font, display_scale, &f);
 
  if (g->GetTextRenderingHint() == Gdiplus::TextRenderingHintClearTypeGridFit) {
    g->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
  } else {
    switch (op) {
    case Operator::SOURCE_OVER:
      g->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
      break;
    case Operator::COPY:
      g->SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
      break;
    }
  }
    
  auto text2 = from_utf8(text);
  int style_bits = 0;
  if (font.weight.isBold()) {
    style_bits |= Gdiplus::FontStyleBold;
  }
  if (font.style == Font::ITALIC) {
    style_bits |= Gdiplus::FontStyleItalic;
  }

  switch (textBaseline) {
  case TextBaseline::TOP: break;
  case TextBaseline::HANGING: break;
  case TextBaseline::MIDDLE: f.SetLineAlignment(Gdiplus::StringAlignmentCenter); break;
  case TextBaseline::BOTTOM: f.SetLineAlignment(Gdiplus::StringAlignmentFar);
  }

  switch (textAlign) {
  case TextAlign::CENTER: f.SetAlignment(Gdiplus::StringAlignmentCenter); break;
  case TextAlign::END: case TextAlign::RIGHT: f.SetAlignment(Gdiplus::StringAlignmentFar); break;
  case TextAlign::START: case TextAlign::LEFT: f.SetAlignment(Gdiplus::StringAlignmentNear); break;
  }

  Gdiplus::FontFamily family(L"Segoe UI Emoji");
  Gdiplus::PointF pntF(x, y);

  switch (mode) {
  case RenderMode::STROKE:
    {
      Gdiplus::GraphicsPath path;
      path.AddString(text2.data(), text2.size(), &family, style_bits, scaled_font_size, pntF, &f);

      Gdiplus::Pen pen(toGDIColor(style.color, globalAlpha), lineWidth * display_scale);
      g->DrawPath(&pen, &path);
    }
    break;
  case RenderMode::FILL:
    {
      Gdiplus::SolidBrush brush(toGDIColor(style.color, globalAlpha));
      if (g->GetTextRenderingHint() == Gdiplus::TextRenderingHintClearTypeGridFit ||
	  g->GetTextRenderingHint() == Gdiplus::TextRenderingHintAntiAliasGridFit) {
	Gdiplus::Font font(&family, scaled_font_size, style_bits, Gdiplus::UnitWorld);
	g->DrawString(text2.data(), text2.size(), &font, pntF, &f, &brush);
      } else {
	Gdiplus::GraphicsPath path;
	path.AddString(text2.data(), text2.size(), &family, style_bits, scaled_font_size, pntF, &f);
	g->FillPath(&brush, &path);       
      }
    }
    break;
  }

  if (!clipPath.empty()) {
    g->ResetClip();
  }
}

TextMetrics
GDIPlusSurface::measureText(const Font & font, const std::string & text, TextBaseline textBaseline, float display_scale) {
  if (!initializeContext()) return TextMetrics();

  Gdiplus::StringFormat f;
  configureFonts(font, display_scale, &f);
    
  auto text2 = from_utf8(text);
  int style = 0;
  if (font.weight.isBold()) {
    style |= Gdiplus::FontStyleBold;
  }
  if (font.style == Font::ITALIC) {
    style |= Gdiplus::FontStyleItalic;
  }

  Gdiplus::FontFamily fontFamily(L"Segoe UI Emoji");

  Gdiplus::Font gdi_font(&fontFamily, font.size * display_scale, style, Gdiplus::UnitPixel);
  Gdiplus::RectF boundingBox;
  Gdiplus::PointF origin(0, 0);
  
  auto orig_unit = g->GetPageUnit();
  g->SetPageUnit(Gdiplus::UnitPixel);

  g->MeasureString(text2.data(), text2.size(), &gdi_font, origin, &f, &boundingBox);

  g->SetPageUnit(orig_unit);
  
  Gdiplus::SizeF size;
  boundingBox.GetSize(&size);
  
  float ascent = gdi_font.GetSize() * fontFamily.GetCellAscent(style) / fontFamily.GetEmHeight(style);
  float descent = gdi_font.GetSize() * fontFamily.GetCellDescent(style) / fontFamily.GetEmHeight(style);
  float baseline = 0;
  if (textBaseline == TextBaseline::MIDDLE) {
    baseline = (ascent + descent) / 2;
  } else if (textBaseline == TextBaseline::TOP) {
    baseline = (ascent + descent);
  }
  
  return TextMetrics(size.Width / display_scale, (descent - baseline) / display_scale, (ascent - baseline) / display_scale);
}

class GDIPlusImage : public Image {
public:
  void setConverter(FilenameConverter * _converter) { converter = _converter; }
  GDIPlusImage(float _display_scale) : Image(_display_scale) { }
  GDIPlusImage(const std::string & filename, float _display_scale) : Image(filename, _display_scale) { }
  GDIPlusImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels, float _display_scale) : Image(_data, _width, _height, _num_channels, _display_scale) { }
  
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

// The locked bits are 32-bit pixels whatever the channel count; one
// channel is the alpha, three drop the unused byte.
std::unique_ptr<ImageData>
GDIPlusSurface::readPixels() {
  unsigned int w = getActualWidth(), h = getActualHeight(), channels = getNumChannels();
  auto r = std::make_unique<ImageData>(w, h, channels);
  r->setFormat(ChannelOrder::BGR, channels == 4);
  auto src = (const unsigned char *)lockMemory(false);
  if (src) {
    size_t stride = size_t(std::abs(data.Stride));
    for (unsigned int y = 0; y < h; y++) {
      const unsigned char * row = src + y * stride;
      unsigned char * out = r->getData() + y * r->getBytesPerRow();
      if (channels == 4) {
	memcpy(out, row, 4 * size_t(w));
      } else {
	for (unsigned int x = 0; x < w; x++) {
	  if (channels == 1) out[x] = row[4 * x + 3];
	  else memcpy(out + 3 * x, row + 4 * x, 3);
	}
      }
    }
  }
  releaseMemory();
  return r;
}

std::unique_ptr<Image>
GDIPlusSurface::createImage(float display_scale) {
  auto image = std::make_unique<GDIPlusImage>(display_scale);
  image->getDataPtr() = readPixels();
  return image;
}

std::unique_ptr<Image>
GDIPlusContextFactory::createImage() {
  return std::make_unique<GDIPlusImage>(getDisplayScale());
}

std::unique_ptr<Image>
GDIPlusContextFactory::loadImage(const std::string & filename) {
  auto image = std::make_unique<GDIPlusImage>(filename, getDisplayScale());
  image->setConverter(converter);
  return image;
}

std::unique_ptr<Image>
GDIPlusContextFactory::createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) {
  return std::make_unique<GDIPlusImage>(_data, _width, _height, _num_channels, getDisplayScale());
}


