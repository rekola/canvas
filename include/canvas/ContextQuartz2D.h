#ifndef _CANVAS_CONTEXTQUARTZ2D_H_
#define _CANVAS_CONTEXTQUARTZ2D_H_

#include "Context.h"

#include <QuartzCore/QuartzCore.h>
#include <CoreText/CoreText.h>
#include "FilenameConverter.h"

#include <TargetConditionals.h>
#include <unordered_map>

namespace canvas {
  class Quartz2DCache {
  public:
    Quartz2DCache() { }
    Quartz2DCache(const Quartz2DCache & other) = delete;
    ~Quartz2DCache() {
#ifdef MEMDEBUG
      // if (colorspace && CFGetRetainCount(colorspace) != 1) std::cerr << "leaking memory A!\n";
#endif
      CGColorSpaceRelease(colorspace);
      CGColorSpaceRelease(colorspace_gray);
      CGColorSpaceRelease(colorspace_float);
      for (auto & fd : fonts) {
#ifdef MEMDEBUG
	// if (CFGetRetainCount(fd.second) != 1) std::cerr << "leaking memory B!\n";
#endif
        CFRelease(fd.second);
      }
    }
      
    Quartz2DCache & operator=(const Quartz2DCache & other) = delete;
    
    CTFontRef getFont(const Font & font, float display_scale) {
      float size = font.size * display_scale;
      bool is_bold = font.weight.isBold();
      bool is_italic = font.style == Font::ITALIC;
      std::string key = font.family + "/" + std::to_string(size) + "/" + (is_bold ? "bold" : "") + "/" + (is_italic ? "italic" : "");
      auto it = fonts.find(key);
      if (it != fonts.end()) return it->second;
      CTFontRef font2 = createFont(font.family, size, is_bold, is_italic);
      fonts[key] = font2;
      return font2;
    }

    CGColorSpaceRef & getColorSpace() {
      if (!colorspace) colorspace = CGColorSpaceCreateDeviceRGB();
      return colorspace;
    }
    // The colour space of a float surface: extended range, so components
    // are not clamped to [0, 1], and linear, so there is no transfer
    // function. Every colour, gradient and image drawn on a float surface
    // is created in this same space, so no colour conversion takes place
    // and the numbers are drawn as given, whatever space the client means
    // them to be in.
    CGColorSpaceRef & getColorSpaceFloat() {
      if (!colorspace_float) colorspace_float = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
      return colorspace_float;
    }
    CGColorSpaceRef & getColorSpaceGray() {
      if (!colorspace_gray) colorspace_gray = CGColorSpaceCreateDeviceGray();
      return colorspace_gray;
    }
    
  private:
    static bool isNamed(CTFontRef font, CFStringRef name) {
      bool found = false;
      CFStringRef names[] = { CTFontCopyFamilyName(font), CTFontCopyPostScriptName(font), CTFontCopyFullName(font) };
      for (CFStringRef n : names) {
        if (!n) continue;
        found = found || CFStringCompare(n, name, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
        CFRelease(n);
      }
      return found;
    }

    // The CSS generic names and any installed family; sans-serif (the default)
    // and a family that is not installed stay Arial.
    static CTFontRef createFont(const std::string & family, float size, bool is_bold, bool is_italic) {
      CTFontRef base = 0;
      if (family == "monospace") {
        base = CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, size, NULL);
      } else if (family == "serif") {
        base = CTFontCreateWithName(CFSTR("TimesNewRomanPSMT"), size, NULL);
      } else if (!family.empty() && family != "sans-serif") {
        CFStringRef name = CFStringCreateWithCString(NULL, family.c_str(), kCFStringEncodingUTF8);
        if (name) {
          base = CTFontCreateWithName(name, size, NULL);
          // CoreText answers an unknown name with a default font: not a match.
          if (base && !isNamed(base, name)) {
            CFRelease(base);
            base = 0;
          }
          CFRelease(name);
        }
      }
      if (base) {
        CTFontSymbolicTraits traits = (is_bold ? kCTFontBoldTrait : 0) | (is_italic ? kCTFontItalicTrait : 0);
        if (traits) {
          CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, size, NULL, traits, traits);
          if (styled) {
            CFRelease(base);
            base = styled;
          }
        }
        return base;
      }
      if (is_italic && is_bold) return CTFontCreateWithName(CFSTR("Arial-BoldItalicMT"), size, NULL);
      if (is_bold) return CTFontCreateWithName(CFSTR("Arial-BoldMT"), size, NULL);
      if (is_italic) return CTFontCreateWithName(CFSTR("Arial-ItalicMT"), size, NULL);
      return CTFontCreateWithName(CFSTR("ArialMT"), size, NULL);
    }

    CGColorSpaceRef colorspace = 0, colorspace_gray = 0, colorspace_float = 0;
    std::unordered_map<std::string, CTFontRef> fonts;
  };
  
  class Quartz2DSurface : public Surface {
  public:
    friend class ContextQuartz2D;
        
  Quartz2DSurface(const std::shared_ptr<Quartz2DCache> & _cache, float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels, PixelType _type = PixelType::UINT8)
    : Surface(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels, _type), cache(_cache) {
      checkPixelType(_num_channels, _type);
      if (_actual_width && _actual_height) {
	size_t bitmapByteCount = size_t(getBytesPerRow()) * _actual_height;
        bitmapData = new unsigned char[bitmapByteCount];
        memset(bitmapData, 0, bitmapByteCount);
        createFloatContext();
      }
  }

    // Float surfaces are 32-bit float RGBA bitmap contexts in an
    // extended linear colour space (macOS; iOS has no such contexts here).
    static bool isFloatSupported() {
#if TARGET_OS_OSX
      return true;
#else
      return false;
#endif
    }
  
    // A float image makes a float surface (bytes make a byte surface).
    Quartz2DSurface(const std::shared_ptr<Quartz2DCache> & _cache, const ImageData & image);  
    
    ~Quartz2DSurface() {
      if (active_shadow_color) {
#ifdef MEMDEBUG
	// if (CFGetRetainCount(active_shadow_color) != 1) std::cerr << "leaking memory C!\n";
#endif
	CGColorRelease(active_shadow_color);
      }
      if (gc) {
#ifdef MEMDEBUG
	// if (CFGetRetainCount(gc) != 1) std::cerr << "leaking memory D!\n";
#endif
	CGContextRelease(gc);
      }
      delete[] bitmapData;
    }

    void * lockMemory(bool write_access = false) override {
      // Drawing is lazy; make sure it has happened before anyone reads.
      if (gc) CGContextFlush(gc);
      return bitmapData;
    }
    
    void releaseMemory() override { }

    void renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, const LineStyle & lineStyle) override;

    void resize(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) override;
    
    void renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) override {
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

      CFStringRef text2 = CFStringCreateWithCString(NULL, text.c_str(), kCFStringEncodingUTF8);
      if (!text2) {
#ifdef MEMDEBUG
        // std::cerr << "failed to create CString from '" << text << "'" << std::endl;
#endif
        return;
      }
      
      CTFontRef font2 = cache->getFont(font, display_scale);
#ifdef MEMDEBUG
      int font_retain = CFGetRetainCount(font2);
      // if (font_retain != 1) std::cerr << "too many retains for font (" << font_retain << ")" << std::endl;
#endif
      CGColorRef color = createCGColor(style.color, globalAlpha);
      
#if 0
      int traits = 0;
      if (font.weight.isBold()) traits |= kCTFontBoldTrait;
      if (font.slant == Font::ITALIC) traits |= kCTFontItalicTrait;
      CFNumberRef traits2 = CFNumberCreate(NULL, kCFNumberSInt32Type, &traits);
#endif
      
      CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName }; // kCTFontSymbolicTrait };
      CFTypeRef values[] = { font2, color }; // traits2
      
      CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, sizeof(keys) / sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      
      CFAttributedStringRef attrString = CFAttributedStringCreate(NULL, text2, attr);
      CTLineRef line = CTLineCreateWithAttributedString(attrString);
      
      double x = p.x * display_scale;
      double y = p.y * display_scale;

      if (textAlign != TextAlign::LEFT || textBaseline != TextBaseline::ALPHABETIC) {
	CGFloat ascent, descent, leading;
	double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
	
	y += getBaselineOffset(textBaseline, ascent, descent);
	
	switch (textAlign) {
        case TextAlign::LEFT: break;
        case TextAlign::CENTER: x -= width / 2; break;
        case TextAlign::RIGHT: x -= width; break;
        default: break;
	}
      }
      
      // CGContextSetTextMatrix(gc, CGAffineTransformIdentity);
      CGContextSetTextMatrix(gc, CGAffineTransformMakeScale(1.0, -1.0)); // Use this one if the view's coordinates are flipped
      CGContextSetTextPosition(gc, x, y);
      CTLineDraw(line, gc);

#ifdef MEMDEBUG
      // if (CFGetRetainCount(line) != 1) std::cerr << "leaking memory F!\n";
#endif
      CFRelease(line);
#ifdef MEMDEBUG
      // if (CFGetRetainCount(attrString) != 1) std::cerr << "leaking memory G!\n";
#endif
      CFRelease(attrString);
#ifdef MEMDEBUG
      // if (CFGetRetainCount(attr) != 1) std::cerr << "leaking memory H!\n";
#endif
      CFRelease(attr);
#ifdef MEMDEBUG
      // if (CFGetRetainCount(text2) != 1) std::cerr << "leaking memory I!\n";
#endif
      CFRelease(text2);
      // CFRelease(traits2);
#ifdef MEMDEBUG
      int color_retain = CFGetRetainCount(color);
      // if (color_retain != 1) std::cerr << "leaking CGColor (" << color_retain << ")!\n";
#endif
      CGColorRelease(color);

      if (has_shadow || !clipPath.empty()) {
        CGContextRestoreGState(gc);
      }
    }

    TextMetrics measureText(const Font & font, const std::string & text, TextBaseline textBaseline, float display_scale) override {
      CTFontRef font2 = cache->getFont(font, display_scale);
      CFStringRef text2 = CFStringCreateWithCString(NULL, text.c_str(), kCFStringEncodingUTF8);
      CFStringRef keys[] = { kCTFontAttributeName };
      CFTypeRef values[] = { font2 };
      CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, sizeof(keys) / sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      
      CFAttributedStringRef attrString = CFAttributedStringCreate(NULL, text2, attr);
      CTLineRef line = CTLineCreateWithAttributedString(attrString);
      
      CGFloat ascent, descent, leading;
      double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
      
#ifdef MEMDEBUG
      // if (CFGetRetainCount(line) != 1) std::cerr << "leaking memory K!\n";
#endif
      CFRelease(line);
#ifdef MEMDEBUG
      // if (CFGetRetainCount(attrString) != 1) std::cerr << "leaking memory L!\n";
#endif
      CFRelease(attrString);
#ifdef MEMDEBUG
      // if (CFGetRetainCount(attr) != 1) std::cerr << "leaking memory M!\n";
#endif
      CFRelease(attr);
#ifdef MEMDEBUG
      // if (CFGetRetainCount(text2) != 1) std::cerr << "leaking memory N!\n";
#endif
      CFRelease(text2);
      
      float offset = getBaselineOffset(textBaseline, ascent, descent);
      return TextMetrics(width / display_scale, (descent + offset) / display_scale, (ascent - offset) / display_scale);
    }

    void drawImage(Surface & surface, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) override {
      initializeContext();
#if 1
      auto img = surface.createImage(displayScale);
      drawImage(img->getData(), p, w, h, displayScale, globalAlpha, shadowBlur, shadowOffsetX, shadowOffsetY, shadowColor, clipPath, imageSmoothingEnabled);
#else
      _img.initializeContext();
      Quartz2DSurface & img = dynamic_cast<Quartz2DSurface &>(_img);
      CGImageRef myImage = CGBitmapContextCreateImage(img.gc);
      CGContextDrawImage(gc, CGRectMake(x, y, w, h), myImage);
      CGImageRelease(myImage);
#endif
    }
    void drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) override;
  
    CGContextRef getContext() { return gc; }
 
  protected:
    std::unique_ptr<ImageData> readPixels() override;
    void sendPath(const Path2D & path, float display_scale);
    // On a float surface the colour is made in the surface's own space, so
    // that nothing is converted or clamped.
    CGColorRef createCGColor(const Color & color, float globalAlpha = 1.0f) {
      CGFloat cv[] = { color.red, color.green, color.blue, color.alpha * globalAlpha};
      return CGColorCreate(isFloat() ? cache->getColorSpaceFloat() : cache->getColorSpace(), cv);
    }
    bool isFloat() const { return getPixelType() == PixelType::FLOAT32; }
    size_t getBytesPerRow() const {
      return getNumChannels() == 1 ? getActualWidth() : size_t(getActualWidth()) * (isFloat() ? 16 : 4);
    }
    static void checkPixelType(unsigned int num_channels, PixelType type) {
      if (type != PixelType::FLOAT32) return;
      if (!isFloatSupported()) throw UnsupportedFormatException("float surfaces need macOS");
      if (num_channels != 4) throw UnsupportedFormatException("float surfaces have 4 channels");
    }
    // A float surface that cannot get its bitmap context is an error now,
    // not a crash at the first draw.
    void createFloatContext() {
      if (!isFloat()) return;
      initializeContext();
      if (!gc) throw UnsupportedFormatException("Quartz refused a 32-bit float bitmap context");
    }
    
    void setShadow(float shadowOffsetX, float shadowOffsetY, float shadowBlur, const Color & shadowColor, float displayScale) {
      initializeContext();
      CGSize offset = CGSizeMake(displayScale * shadowOffsetX, displayScale * shadowOffsetY);
      if (active_shadow_color) {
#ifdef MEMDEBUG
	// if (CFGetRetainCount(active_shadow_color) != 1) std::cerr << "leaking memory Q!\n";
#endif
	CGColorRelease(active_shadow_color);
      }
      active_shadow_color = createCGColor(shadowColor);
      CGContextSetShadowWithColor(gc, offset, displayScale * shadowBlur, active_shadow_color);
    }
    
    void initializeContext() {
      if (!gc && bitmapData) {
	unsigned int bitmapBytesPerRow;
	CGBitmapInfo bitmapInfo = 0;
	// The memory is B, G, R, A (premultiplied) like the other backends;
	// one channel is an alpha mask.
	CGColorSpaceRef space = cache->getColorSpace();
	int bitsPerComponent = 8;
	if (isFloat()) {
	  // Premultiplied R, G, B, A as 32-bit floats, in the extended
	  // linear space: the colour space of every colour drawn here.
	  space = cache->getColorSpaceFloat();
	  bitsPerComponent = 32;
	  bitmapBytesPerRow = getActualWidth() * 16;
	  bitmapInfo = kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Little;
	} else if (getNumChannels() == 1) {
	  bitmapBytesPerRow = getActualWidth();
	  bitmapInfo |= kCGImageAlphaOnly;
	  space = 0;
	} else if (getNumChannels() == 3) {
	  bitmapBytesPerRow = getActualWidth() * 4;
	  bitmapInfo |= kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little;
	} else if (getNumChannels() == 4) {
	  bitmapBytesPerRow = getActualWidth() * 4;
	  bitmapInfo |= kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
	} else {
	  return;
	}
        gc = CGBitmapContextCreate(bitmapData, getActualWidth(), getActualHeight(), bitsPerComponent, bitmapBytesPerRow, space, bitmapInfo);
        if (!gc) return;
        CGContextSetInterpolationQuality(gc, kCGInterpolationHigh);
        CGContextSetShouldAntialias(gc, true);
	flipY();
      }
    }

    void flipY() {
      CGContextTranslateCTM(gc, 0, getActualHeight());
      CGContextScaleCTM(gc, 1.0, -1.0);
    }

    std::unique_ptr<Image> createImage(float display_scale) override;
    
  private:
    std::shared_ptr<Quartz2DCache> cache;
    CGContextRef gc = 0;
    unsigned char * bitmapData = 0;
    CGColorRef active_shadow_color = 0;
  };

  class ContextQuartz2D : public Context {
  public:
  ContextQuartz2D(const std::shared_ptr<Quartz2DCache> & _cache, unsigned int _width, unsigned int _height, unsigned int _num_channels, float _display_scale, PixelType _type = PixelType::UINT8)
    : Context(_display_scale),
      cache(_cache),
      default_surface(_cache, _width, _height, toPixels(_width, _display_scale), toPixels(_height, _display_scale), _num_channels, _type)
      {
      }
    // A default surface of exactly size pixels; the logical size is
    // size / _display_scale.
    ContextQuartz2D(const std::shared_ptr<Quartz2DCache> & _cache, PixelSize size, unsigned int _num_channels, float _display_scale, PixelType _type = PixelType::UINT8)
    : Context(_display_scale),
      cache(_cache),
      default_surface(_cache, size.width / _display_scale, size.height / _display_scale, size.width, size.height, _num_channels, _type)
      {
      }

    std::unique_ptr<Surface> createSurface(const ImageData & image) override {
        return std::unique_ptr<Surface>(new Quartz2DSurface(cache, image));
    }
    std::unique_ptr<Surface> createSurface(unsigned int _width, unsigned int _height, unsigned int _num_channels) override {
      return std::unique_ptr<Surface>(new Quartz2DSurface(cache, _width, _height, toPixels(_width, getDisplayScale()), toPixels(_height, getDisplayScale()), _num_channels));
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int _num_channels) override {
      return std::unique_ptr<Surface>(new Quartz2DSurface(cache, size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, _num_channels));
    }
        
    bool supportsPixelType(PixelType type) const override {
      return type == PixelType::UINT8 || Quartz2DSurface::isFloatSupported();
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int _num_channels, PixelType type) override {
      return std::unique_ptr<Surface>(new Quartz2DSurface(cache, size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, _num_channels, type));
    }

    Surface & getDefaultSurface() override { return default_surface; }
    const Surface & getDefaultSurface() const override { return default_surface; }
    
  protected:
    bool hasNativeShadows() const override { return true; }
    bool hasNativeEmoticons() const override { return true; }
    
  private:
    std::shared_ptr<Quartz2DCache> cache;
    Quartz2DSurface default_surface;
  };

  class Quartz2DContextFactory : public ContextFactory {
  public:
    Quartz2DContextFactory(float _display_scale, FilenameConverter * _converter) : ContextFactory(_display_scale), converter(_converter) {
      cache = std::make_shared<Quartz2DCache>();
  }
    std::unique_ptr<Context> createContext(unsigned int width, unsigned int height, unsigned int num_channels) override {
      return std::unique_ptr<Context>(new ContextQuartz2D(cache, width, height, num_channels, getDisplayScale()));
    }
    std::unique_ptr<Surface> createSurface(unsigned int width, unsigned int height, unsigned int num_channels) override {
      return std::unique_ptr<Surface>(new Quartz2DSurface(cache, width, height, toPixels(width, getDisplayScale()), toPixels(height, getDisplayScale()), num_channels));
    }
    std::unique_ptr<Context> createContext(PixelSize size, unsigned int num_channels) override {
      return std::unique_ptr<Context>(new ContextQuartz2D(cache, size, num_channels, getDisplayScale()));
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int num_channels) override {
      return std::unique_ptr<Surface>(new Quartz2DSurface(cache, size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, num_channels));
    }

    bool supportsPixelType(PixelType type) const override {
      return type == PixelType::UINT8 || Quartz2DSurface::isFloatSupported();
    }
    std::unique_ptr<Context> createContext(PixelSize size, unsigned int num_channels, PixelType type) override {
      return std::unique_ptr<Context>(new ContextQuartz2D(cache, size, num_channels, getDisplayScale(), type));
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int num_channels, PixelType type) override {
      return std::unique_ptr<Surface>(new Quartz2DSurface(cache, size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, num_channels, type));
    }
    std::unique_ptr<Image> loadImage(const std::string & filename) override;
    std::unique_ptr<Image> createImage() override;
    std::unique_ptr<Image> createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) override;

  private:
    std::shared_ptr<Quartz2DCache> cache;
    FilenameConverter * converter;
  };
};

#endif
