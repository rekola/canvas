#include <canvas/ContextQuartz2D.h>

#include <algorithm>
#include <cassert>

#include <ImageIO/ImageIO.h>

using namespace canvas;
using namespace std;

// Pixels as B, G, R, A premultiplied (Quartz's kCGImageAlphaPremultipliedFirst
// with kCGBitmapByteOrder32Little), whatever format the image is in. One
// channel is an alpha mask and stays one channel.
static std::unique_ptr<unsigned char[]> toNativePixels(const ImageData & image) {
  size_t n = size_t(image.getWidth()) * image.getHeight();
  unsigned int channels = image.getNumChannels();
  if (channels == 1) {
    auto out = std::make_unique<unsigned char[]>(n);
    memcpy(out.get(), image.getData(), n);
    return out;
  }
  auto out = std::make_unique<unsigned char[]>(4 * n);
  bool bgr = image.getChannelOrder() == ChannelOrder::BGR;
  bool premultiplied = image.isPremultiplied();
  for (size_t i = 0; i < n; i++) {
    const unsigned char * p = image.getData() + channels * i;
    unsigned int r = bgr ? p[2] : p[0], g = p[1], b = bgr ? p[0] : p[2], a = 255;
    if (channels == 4) {
      a = p[3];
      if (!premultiplied) {
	r = (r * a + 127) / 255;
	g = (g * a + 127) / 255;
	b = (b * a + 127) / 255;
      }
    }
    out[4 * i + 0] = (unsigned char)b;
    out[4 * i + 1] = (unsigned char)g;
    out[4 * i + 2] = (unsigned char)r;
    out[4 * i + 3] = (unsigned char)a;
  }
  return out;
}

Quartz2DSurface::Quartz2DSurface(const std::shared_ptr<Quartz2DCache> & _cache, const ImageData & image)
  : Surface(image.getWidth(), image.getHeight(), image.getWidth(), image.getHeight(), image.getNumChannels()), cache(_cache) {
  assert(getActualWidth() && getActualHeight());
  size_t bitmapByteCount = (getNumChannels() == 1 ? 1 : 4) * size_t(getActualWidth()) * getActualHeight();
  bitmapData = new unsigned char[bitmapByteCount];
  memcpy(bitmapData, toNativePixels(image).get(), bitmapByteCount);
}

// Three channels are stored as 32-bit pixels; ImageData's are not.
std::unique_ptr<ImageData>
Quartz2DSurface::readPixels() {
  unsigned int w = getActualWidth(), h = getActualHeight(), channels = getNumChannels();
  auto r = std::make_unique<ImageData>(w, h, channels);
  r->setFormat(ChannelOrder::BGR, channels == 4);
  auto src = (const unsigned char *)lockMemory(false);
  if (src) {
    if (channels == 3) {
      for (size_t i = 0; i < size_t(w) * h; i++) memcpy(r->getData() + 3 * i, src + 4 * i, 3);
    } else {
      memcpy(r->getData(), src, r->calculateSize());
    }
  }
  releaseMemory();
  return r;
}

void
Quartz2DSurface::sendPath(const Path2D & path, float scale) {
  initializeContext();
  CGContextBeginPath(gc);
  for (auto pc : path.getData()) {
    switch (pc.type) {
    case PathComponent::MOVE_TO: CGContextMoveToPoint(gc, pc.x0 * scale, pc.y0 * scale); break;
    case PathComponent::LINE_TO: CGContextAddLineToPoint(gc, pc.x0 * scale, pc.y0 * scale); break;
    case PathComponent::ARC: CGContextAddArc(gc, pc.x0 * scale, pc.y0 * scale, pc.radius * scale, pc.sa, pc.ea, pc.anticlockwise); break;
    case PathComponent::CLOSE: CGContextClosePath(gc); break;
    }
  }
}

void
Quartz2DSurface::renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, const LineStyle & lineStyle) {
  initializeContext();

  bool has_shadow = shadowBlur > 0.0f || shadowOffsetX != 0.0f || shadowOffsetY != 0.0f;
  if (has_shadow || !clipPath.empty()) {
    CGContextSaveGState(gc);
  }
  if (!clipPath.empty()) {
    sendPath(clipPath, display_scale);
    CGContextClip(gc);
  }
  if (has_shadow) {
    setShadow(shadowOffsetX, shadowOffsetY, shadowBlur, shadowColor, display_scale);
  }
  
  switch (op) {
  case Operator::SOURCE_OVER:
    CGContextSetBlendMode(gc, kCGBlendModeNormal);
    break;
  case Operator::COPY:
    CGContextSetBlendMode(gc, kCGBlendModeCopy);
    break;
  }
  switch (mode) {
  case RenderMode::STROKE:
    sendPath(path, display_scale);
    CGContextSetRGBStrokeColor(gc, style.color.red,
			       style.color.green,
			       style.color.blue,
			       style.color.alpha * globalAlpha);
    CGContextSetLineWidth(gc, lineWidth * display_scale);
    switch (lineStyle.cap) {
    case LineCap::BUTT: CGContextSetLineCap(gc, kCGLineCapButt); break;
    case LineCap::ROUND: CGContextSetLineCap(gc, kCGLineCapRound); break;
    case LineCap::SQUARE: CGContextSetLineCap(gc, kCGLineCapSquare); break;
    }
    switch (lineStyle.join) {
    case LineJoin::MITER: CGContextSetLineJoin(gc, kCGLineJoinMiter); break;
    case LineJoin::ROUND: CGContextSetLineJoin(gc, kCGLineJoinRound); break;
    case LineJoin::BEVEL: CGContextSetLineJoin(gc, kCGLineJoinBevel); break;
    }
    CGContextSetMiterLimit(gc, lineStyle.miterLimit);
    if (!lineStyle.dash.empty()) {
      std::vector<CGFloat> dash;
      for (auto & v : lineStyle.dash) dash.push_back(transformation.transformSize(v) * display_scale);
      CGContextSetLineDash(gc, transformation.transformSize(lineStyle.dashOffset) * display_scale, dash.data(), dash.size());
    } else {
      CGContextSetLineDash(gc, 0, NULL, 0);
    }
    CGContextStrokePath(gc);  
    break;
  case RenderMode::FILL:
    if ((style.getType() == Style::LINEAR_GRADIENT || style.getType() == Style::RADIAL_GRADIENT) && !style.getColors().empty()) {
      // Every stop, padded with the end colours beyond the end points.
      const std::map<float, Color> & colors = style.getColors();
      std::vector<CGFloat> locations, components;
      for (auto & stop : colors) {
	locations.push_back(std::min(1.0f, std::max(0.0f, stop.first)));
	components.push_back(stop.second.red);
	components.push_back(stop.second.green);
	components.push_back(stop.second.blue);
	components.push_back(stop.second.alpha * globalAlpha);
      }
      CGGradientRef myGradient = CGGradientCreateWithColorComponents(cache->getColorSpace(), components.data(), locations.data(), locations.size());

      CGContextSaveGState(gc);
      sendPath(path, display_scale);
      CGContextClip(gc);

      CGPoint myStartPoint = CGPointMake(style.p0.x * display_scale, style.p0.y * display_scale);
      CGPoint myEndPoint = CGPointMake(style.p1.x * display_scale, style.p1.y * display_scale);
      CGGradientDrawingOptions pad = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
      if (style.getType() == Style::RADIAL_GRADIENT) {
	CGContextDrawRadialGradient(gc, myGradient, myStartPoint, style.r0 * display_scale, myEndPoint, style.r1 * display_scale, pad);
      } else {
	CGContextDrawLinearGradient(gc, myGradient, myStartPoint, myEndPoint, pad);
      }

      CGContextRestoreGState(gc);

#ifdef MEMDEBUG
      // if (CFGetRetainCount(myGradient) != 1) cerr << "leaking memory 7!\n";
#endif
      CGGradientRelease(myGradient);
    } else {
      sendPath(path, display_scale);
      CGContextSetRGBFillColor(gc, style.color.red,
			       style.color.green,
			       style.color.blue,
			       style.color.alpha * globalAlpha);
      CGContextFillPath(gc);
    }
  }
  if (op != Operator::SOURCE_OVER) {
    CGContextSetBlendMode(gc, kCGBlendModeNormal);
  }
  if (has_shadow || !clipPath.empty()) {
    CGContextRestoreGState(gc);
  }
}

void
Quartz2DSurface::resize(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) {
  Surface::resize(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels);
  
  if (gc) {
#ifdef MEMDEBUG
    // if (CFGetRetainCount(gc) != 1) std::cerr << "leaking memory E!\n";
#endif
    CGContextRelease(gc);
    gc = 0;
  }
  delete[] bitmapData;
  
  assert(getActualWidth() && getActualHeight());
  unsigned int bitmapByteCount = 4 * getActualWidth() * getActualHeight();
  bitmapData = new unsigned char[bitmapByteCount];
  memset(bitmapData, 0, bitmapByteCount);
}

void
Quartz2DSurface::drawImage(const ImageData & input, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled) {
  if (input.getNumChannels() < 1 || input.getNumChannels() > 4 || input.getNumChannels() == 2) return;
  // Always drawn from native B, G, R, A premultiplied pixels (a mask is black with that alpha).
  std::unique_ptr<unsigned char[]> native = toNativePixels(input);
  size_t bytes_per_row = 4 * size_t(input.getWidth());
  std::unique_ptr<unsigned char[]> with_color;
  if (input.getNumChannels() == 1) {
    size_t n = size_t(input.getWidth()) * input.getHeight();
    with_color = std::make_unique<unsigned char[]>(4 * n);
    for (size_t i = 0; i < n; i++) with_color[4 * i + 3] = native[i];
    native = std::move(with_color);
  }
  CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
  CGColorSpaceRef colorspace = cache->getColorSpace();

  initializeContext();
  
  bool has_shadow = shadowBlur > 0.0f || shadowOffsetX != 0.0f || shadowOffsetY != 0.0f;
  if (has_shadow || !clipPath.empty()) {
    CGContextSaveGState(gc);
  }
  if (!clipPath.empty()) {
    sendPath(clipPath, displayScale);
    CGContextClip(gc);
  }
  if (has_shadow) {
    setShadow(shadowOffsetX, shadowOffsetY, shadowBlur, shadowColor, displayScale);
  }
  int bitsPerComponent = 8;
  int bitsPerPixel = 32;
  auto cfdata = CFDataCreate(0, native.get(), input.getHeight() * bytes_per_row);
  auto provider = CGDataProviderCreateWithCFData(cfdata);
  auto img = CGImageCreate(input.getWidth(), input.getHeight(), bitsPerComponent, bitsPerPixel, bytes_per_row, colorspace, bitmapInfo, provider, 0, imageSmoothingEnabled, kCGRenderingIntentDefault);
  if (img) {
    CGContextSetInterpolationQuality(gc, imageSmoothingEnabled ? kCGInterpolationHigh : kCGInterpolationNone);
    flipY();
    if (globalAlpha < 1.0f) CGContextSetAlpha(gc, globalAlpha);
    CGContextDrawImage(gc, CGRectMake(displayScale * p.x, getActualHeight() - displayScale * (p.y + h), displayScale * w, displayScale * h), img);
    if (globalAlpha < 1.0f) CGContextSetAlpha(gc, 1.0f);
    flipY();
  
#ifdef MEMDEBUG
    // if (CFGetRetainCount(img) != 1) std::cerr << "leaking memory O!\n";
#endif
    CGImageRelease(img);
#ifdef MEMDEBUG
    // if (CFGetRetainCount(provider) != 1) std::cerr << "leaking memory P!\n";
#endif
  } else {
    // cerr << "failed to create CGImage\n";
  }
  CGDataProviderRelease(provider);
  CFRelease(cfdata);
  if (has_shadow || !clipPath.empty()) {
    CGContextRestoreGState(gc);
  }
}

class Quartz2DImage : public Image {
public:
  Quartz2DImage(FilenameConverter * _converter, float _display_scale)
    : Image(_display_scale), converter(_converter) { }

  Quartz2DImage(FilenameConverter * _converter, const std::string & _filename, float _display_scale)
    : Image(_filename, _display_scale),
      converter(_converter) { }

  Quartz2DImage(FilenameConverter * _converter, const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels, float _display_scale) : Image(_data, _width, _height, _num_channels, _display_scale), converter(_converter) { }
  
  void loadFile() override {
#if 0
    // Create CFString
    CFStringRef filename2 = CFStringCreateWithCString(NULL, filename.c_str(), kCFStringEncodingUTF8);

    // Get a reference to the main bundle
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (!mainBundle) {
      // cerr << "failed to get main bundle, err = " << fnfErr << endl;
    }
    
    // Get a reference to the file's URL
    CFURLRef imageURL = CFBundleCopyResourceURL(mainBundle, filename2, NULL, NULL);

    // Convert the URL reference into a string reference
    CFStringRef imagePath = CFURLCopyFileSystemPath(imageURL, kCFURLPOSIXPathStyle);
    
    // Get the system encoding method
    CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
    
    // Convert the string reference into a C string
    const char * tmp = CFStringGetCStringPtr(imagePath, encodingMethod);
    string path = tmp;

    CFRelease(imagePath);
    CFRelease(imageURL);
    CFRelease(mainBundle);
    CFRelease(filename2);
#else
    string path = filename;
    if (converter && !converter->convert(filename, path)) path = filename;
#endif
    data = loadFromFile(path);
    if (!data.get()) filename.clear();
  }
      
    private:
      FilenameConverter * converter;
};

std::unique_ptr<Image>
Quartz2DContextFactory::loadImage(const std::string & filename) {
  return std::unique_ptr<Image>(new Quartz2DImage(converter, filename, getDisplayScale()));
}

std::unique_ptr<Image>
Quartz2DContextFactory::createImage() {
  return std::unique_ptr<Image>(new Quartz2DImage(converter, getDisplayScale()));
}

std::unique_ptr<Image>
Quartz2DContextFactory::createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) {
  return std::unique_ptr<Image>(new Quartz2DImage(converter, _data, _width, _height, _num_channels, getDisplayScale()));
}

std::unique_ptr<Image>
Quartz2DSurface::createImage(float display_scale) {
  auto image = std::unique_ptr<Image>(new Quartz2DImage(0, display_scale));
  image->getDataPtr() = readPixels();
  return image;
}
