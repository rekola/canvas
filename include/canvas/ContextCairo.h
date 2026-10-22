
#ifndef _CANVAS_CONTEXTCAIRO_H_
#define _CANVAS_CONTEXTCAIRO_H_

#include "Context.h"
#include "FilenameConverter.h"

#include <cairo/cairo.h>

namespace canvas {
  class ContextCairo;

  // A Cairo image surface. With PixelType::FLOAT32 it is CAIRO_FORMAT_RGBA128F
  // (4 channels; Cairo 1.17.2 or later, check CairoSurface::isFloatSupported()).
  //
  // Cairo clamps every colour it is given (solid colours, gradient stops, and
  // float image sources) to [0, 1], though it stores and blends values above 1
  // in the surface. So a float surface keeps its pixels divided by a
  // "headroom", a power of two that grows to cover the largest colour drawn so
  // far (the stored pixels are rescaled when it does; dividing by a power of
  // two is exact). The scaling is invisible except in lockMemory() and
  // getCairoSurface(), which see the stored values: multiply the colour
  // channels by getHeadroom() to get the drawn ones. getImageData() and
  // createImage() do that.
  class CairoSurface : public Surface {
  public:
    friend class ContextCairo;

    CairoSurface(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels, PixelType _type = PixelType::UINT8);
    // A float image makes a float surface (bytes make a byte surface); one
    // and three channel float images become RGBA (a mask is black with that
    // alpha).
    CairoSurface(const ImageData & image);
    CairoSurface(const CairoSurface & other) = delete;
    CairoSurface(const unsigned char * buffer, size_t size);
    ~CairoSurface();
    
    void resize(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) override;

    void renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, const LineStyle & lineStyle) override;
    void renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) override;
    TextMetrics measureText(const Font & font, const std::string & text, TextBaseline textBaseline, float displayScale) override;
    void drawImage(Surface & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) override;
    void drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) override;

    std::unique_ptr<Image> createImage(float display_scale) override;

    cairo_surface_t * getCairoSurface() { return surface; }

    // Whether this Cairo (compiled against and running) has RGBA128F.
    static bool isFloatSupported();
    // Float surfaces only: stored colour * headroom = drawn colour. 1 for
    // byte surfaces.
    float getHeadroom() const { return headroom; }
    
  protected:
    size_t getMemoryStride() const override { return surface ? cairo_image_surface_get_stride(surface) : 0; }
    std::unique_ptr<ImageData> readPixels() override;

    void flush();
    void markDirty();

    void * lockMemory(bool write_access = false) override {
      flush();
      locked_for_write = write_access;
      return cairo_image_surface_get_data(surface);
    }
    void releaseMemory() override {
      if (locked_for_write) {
	locked_for_write = false;
	markDirty();
      }
    }

    // For drawing an image into a surface of a given headroom (0: the
    // smallest that holds the image).
    CairoSurface(const ImageData & image, PixelType type, float _headroom);

    void initializeContext() {
      if (!cr) {
	if (!surface) {
	  surface = cairo_image_surface_create(isFloat() ? getFloatFormat() : CAIRO_FORMAT_ARGB32, 4, 4);
	}
	cr = cairo_create(surface);	
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
      }
    }

    bool isFloat() const { return getPixelType() == PixelType::FLOAT32; }
    static cairo_format_t getFloatFormat();
    // Raises the headroom to hold colours up to max_component, rescaling the pixels.
    void ensureHeadroom(float max_component);
    void setSolidSource(const Color & color, float alpha);
    void drawNativeSurface(CairoSurface & img, const Point & p, double w, double h, float displayScale, float globalAlpha, const Path2D & clipPath, bool imageSmoothingEnabled);

    void sendPath(const Path2D & path, float displayScale);

  private:
    cairo_t * cr = 0;
    cairo_surface_t * surface;
    unsigned int * storage = 0;
    bool locked_for_write = false;
    float headroom = 1.0f;
  };

  class ContextCairo : public Context {
  public:
  ContextCairo(unsigned int _width, unsigned int _height, unsigned int _num_channels = 4, float _displayScale = 1.0f, PixelType _type = PixelType::UINT8)
      : Context(_displayScale),
      default_surface(_width, _height, toPixels(_width, _displayScale), toPixels(_height, _displayScale), _num_channels, _type)
	{ 
	}
    // A default surface of exactly size pixels; the logical size is
    // size / _displayScale.
    ContextCairo(PixelSize size, unsigned int _num_channels = 4, float _displayScale = 1.0f, PixelType _type = PixelType::UINT8)
      : Context(_displayScale),
      default_surface(size.width / _displayScale, size.height / _displayScale, size.width, size.height, _num_channels, _type)
	{
	}
    
    std::unique_ptr<Surface> createSurface(const ImageData & image) override {
      return std::unique_ptr<Surface>(new CairoSurface(image));
    }
    std::unique_ptr<Surface> createSurface(unsigned int _width, unsigned int _height, unsigned int _num_channels = 4) override {
      return std::unique_ptr<Surface>(new CairoSurface(_width, _height, toPixels(_width, getDisplayScale()), toPixels(_height, getDisplayScale()), _num_channels));
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int _num_channels = 4) override {
      return std::unique_ptr<Surface>(new CairoSurface(size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, _num_channels));
    }

    bool supportsPixelType(PixelType type) const override {
      return type == PixelType::UINT8 || CairoSurface::isFloatSupported();
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int _num_channels, PixelType type) override {
      return std::unique_ptr<Surface>(new CairoSurface(size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, _num_channels, type));
    }

    CairoSurface & getDefaultSurface() override { return default_surface; }
    const CairoSurface & getDefaultSurface() const override { return default_surface; }
    
  protected:
    CairoSurface default_surface;
  };

  class CairoContextFactory : public ContextFactory {
  public:
    // The converter turns an image name into a path; without one the name is the path.
    CairoContextFactory(float display_scale, FilenameConverter * _converter = nullptr) : ContextFactory(display_scale), converter(_converter) { }
    std::unique_ptr<Context> createContext(unsigned int width, unsigned int height, unsigned int num_channels) override {
      return std::unique_ptr<Context>(new ContextCairo(width, height, num_channels, getDisplayScale()));
    }
    std::unique_ptr<Surface> createSurface(unsigned int width, unsigned int height, unsigned int num_channels) override {
      return std::unique_ptr<Surface>(new CairoSurface(width, height, toPixels(width, getDisplayScale()), toPixels(height, getDisplayScale()), num_channels));
    }
    std::unique_ptr<Context> createContext(PixelSize size, unsigned int num_channels) override {
      return std::unique_ptr<Context>(new ContextCairo(size, num_channels, getDisplayScale()));
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int num_channels) override {
      return std::unique_ptr<Surface>(new CairoSurface(size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, num_channels));
    }
    bool supportsPixelType(PixelType type) const override {
      return type == PixelType::UINT8 || CairoSurface::isFloatSupported();
    }
    std::unique_ptr<Context> createContext(PixelSize size, unsigned int num_channels, PixelType type) override {
      return std::unique_ptr<Context>(new ContextCairo(size, num_channels, getDisplayScale(), type));
    }
    std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int num_channels, PixelType type) override {
      return std::unique_ptr<Surface>(new CairoSurface(size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, num_channels, type));
    }
    std::unique_ptr<Image> loadImage(const std::string & filename) override;
    std::unique_ptr<Image> createImage() override;
    std::unique_ptr<Image> createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) override;

  private:
    FilenameConverter * converter;
  };
};

#endif
