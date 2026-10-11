#include <ContextGDIPlus.h>

#include <cassert>

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

GDIPlusSurface::GDIPlusSurface(const std::string & filename) : Surface(0, 0, 0, 0, false) {
  auto tmp = from_utf8(filename);
  bitmap = std::unique_ptr<Gdiplus::Bitmap>(Gdiplus::Bitmap::FromFile(tmp.data()));
  Surface::resize(bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetWidth(), bitmap->GetHeight(), true);
}

GDIPlusSurface::GDIPlusSurface(const unsigned char * buffer, size_t size) : Surface(0, 0, 0, 0, false) {
	assert(0);
}


void
GDIPlusSurface::renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & input_path, const Style & style, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color& shadowColor, const Path2D& clipPath, const std::vector<float> & lineDash) {
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

  switch (mode) {
  case RenderMode::STROKE:
    {
      Gdiplus::Pen pen(toGDIColor(style.color, globalAlpha), lineWidth * display_scale);
      if (!lineDash.empty()) {
	// no matrix transformation for dash (it looks weird)
	std::vector<float> dash;
	for (auto & v : lineDash) dash.push_back(v * display_scale);
	pen.SetDashStyle(Gdiplus::DashStyleCustom);
	pen.SetDashPattern(dash.data(), dash.size());
      }
      g->DrawPath(&pen, &path);
    }
    break;
  case RenderMode::FILL:
    if (style.getType() == Style::LINEAR_GRADIENT) {
      const std::map<float, Color> & colors = style.getColors();
      if (!colors.empty()) {
	auto it0 = colors.begin(), it1 = colors.end();
	it1--;
	const Color & c0 = it0->second, c1 = it1->second;
	Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(Gdiplus::REAL(style.p0.x * display_scale), Gdiplus::REAL(style.p0.y * display_scale)),
					   Gdiplus::PointF(Gdiplus::REAL(style.p1.x * display_scale), Gdiplus::REAL(style.p1.y * display_scale)),
					   toGDIColor(c0, globalAlpha),
					   toGDIColor(c1, globalAlpha));
	g->FillPath(&brush, &path);
      }
    } else {
      Gdiplus::SolidBrush brush(toGDIColor(style.color, globalAlpha));
      g->FillPath(&brush, &path);
    }
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
  if (globalAlpha < 1.0f && 0) {
#if 0
    ImageAttributes  imageAttributes;
    ColorMatrix colorMatrix = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, alpha, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
   
    imageAttributes.SetColorMatrix( &colorMatrix, 
				    ColorMatrixFlagsDefault,
				    ColorAdjustTypeBitmap);
    graphics.DrawImage( &(*(img.bitmap)),
			Gdiplus::Rect(p.x, p.y, w, h), // destination rectangle 
			0, 0,        // upper-left corner of source rectangle 
			getWidth(),       // width of source rectangle
			getHeight(),      // height of source rectangle
			Gdiplus::UnitPixel,
			&imageAttributes);
#endif
  } else if (img.getActualWidth() == (unsigned int)w && img.getActualHeight() == (unsigned int)h && 0) { // this scales image weirdly
    g->DrawImage(&(*(img.bitmap)), Gdiplus::REAL(p.x * displayScale), Gdiplus::REAL(p.y * displayScale));
  } else {
    g->DrawImage(&(*(img.bitmap)), Gdiplus::REAL(p.x * displayScale), Gdiplus::REAL(p.y * displayScale), Gdiplus::REAL(w * displayScale), Gdiplus::REAL(h * displayScale));
  }
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

std::unique_ptr<Image>
GDIPlusSurface::createImage(float display_scale) {
  auto buffer = (unsigned char *)lockMemory(false);
  assert(buffer);

  auto image = std::make_unique<GDIPlusImage>(buffer, getActualWidth(), getActualHeight(), getNumChannels(), display_scale);
  releaseMemory();
  
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


