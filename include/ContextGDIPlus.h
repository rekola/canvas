#ifndef _CANVAS_CONTEXTGDIPLUS_H_
#define _CANVAS_CONTEXTGDIPLUS_H_

#include "Context.h"
#include <FilenameConverter.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
namespace Gdiplus
{
  using std::min;
  using std::max;
};

#undef WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gdiplus.h>

#include <vector>
#include <cstring>

#pragma comment (lib, "gdiplus.lib")

namespace canvas {
  class GDIPlusSurface : public Surface {
  public:
    friend class ContextGDIPlus;

    static inline Gdiplus::PixelFormat getPixelFormat(int num_channels) {
      switch (num_channels) {
      case 1: return PixelFormat32bppPARGB;  // an alpha mask: the alpha of a 32-bit pixel
      case 3: return PixelFormat32bppRGB;
      case 4: return PixelFormat32bppPARGB;
      }
      return PixelFormat32bppPARGB;
    }
    
    GDIPlusSurface(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels)
      : Surface(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels) {
      if (_actual_width && _actual_height) {
	size_t numPixels = _actual_width * _actual_height;
	storage = new BYTE[4 * numPixels];
	memset(storage, 0, 4 * numPixels);
	bitmap = std::make_unique<Gdiplus::Bitmap>(_actual_width, _actual_height, 4 * _actual_width, getPixelFormat(_num_channels), storage);
      }
    }
    GDIPlusSurface(const std::string & filename);
    GDIPlusSurface(const unsigned char * buffer, size_t size);
    // The surface's pixels are premultiplied BGRA words (the alpha of a
    // one channel image, with black colour); the image is converted from
    // whatever format it says it is in.
    GDIPlusSurface(const ImageData & image) : Surface(image.getWidth(), image.getHeight(), image.getWidth(), image.getHeight(), image.getNumChannels())
    {
      size_t numPixels = size_t(image.getWidth()) * image.getHeight();
      unsigned int channels = image.getNumChannels();
      bool bgr = image.getChannelOrder() == ChannelOrder::BGR, premultiplied = image.isPremultiplied();
      storage = new BYTE[4 * numPixels];
      const unsigned char * in = image.getData();
      for (size_t i = 0; i < numPixels; i++) {
	const unsigned char * p = in + channels * i;
	unsigned int r = 0, g = 0, b = 0, a = 255;
	if (channels == 1) {
	  a = p[0];
	} else {
	  r = bgr ? p[2] : p[0];
	  g = p[1];
	  b = bgr ? p[0] : p[2];
	  if (channels == 4) {
	    a = p[3];
	    if (!premultiplied) {
	      r = (r * a + 127) / 255;
	      g = (g * a + 127) / 255;
	      b = (b * a + 127) / 255;
	    }
	  }
	}
	storage[4 * i + 0] = (BYTE)b;
	storage[4 * i + 1] = (BYTE)g;
	storage[4 * i + 2] = (BYTE)r;
	storage[4 * i + 3] = (BYTE)a;
      }
      bitmap = std::unique_ptr<Gdiplus::Bitmap>(new Gdiplus::Bitmap(image.getWidth(), image.getHeight(), image.getWidth() * 4, getPixelFormat(channels), storage));
    }
    ~GDIPlusSurface() {
      delete[] storage;
    }
    void resize(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) override {
      Surface::resize(_logical_width, _logical_height, _actual_width, _actual_height, _num_channels);
      bitmap = std::unique_ptr<Gdiplus::Bitmap>(new Gdiplus::Bitmap(_actual_width, _actual_height, getPixelFormat(_num_channels)));
      g = std::unique_ptr<Gdiplus::Graphics>(nullptr);
    }

    void renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) override;
    TextMetrics measureText(const Font& font, const std::string& text, TextBaseline textBaseline, float display_scale) override;
    
    void renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float display_scale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color& shadowColor, const Path2D& clipPath, const LineStyle & lineStyle) override;
    
    void drawImage(Surface& _img, const Point& p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color& shadowColor, const Path2D& clipPath, bool imageSmoothingEnabled = true) override {
      GDIPlusSurface* img = dynamic_cast<GDIPlusSurface*>(&_img);
      if (img) {
	drawNativeSurface(*img, p, w, h, displayScale, globalAlpha, imageSmoothingEnabled);
      } else {
	auto img2 = _img.createImage(displayScale);
	GDIPlusSurface cs(img2->getData());
	drawNativeSurface(cs, p, w, h, displayScale, globalAlpha, imageSmoothingEnabled);
      }
    }

    void drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color& shadowColor, const Path2D& clipPath, bool imageSmoothingEnabled = true) override {
      GDIPlusSurface cs(_img);
      drawNativeSurface(cs, p, w, h, displayScale, globalAlpha, imageSmoothingEnabled);
    }
    
    std::unique_ptr<Image> createImage(float display_scale) override;
    
  protected:
    std::unique_ptr<ImageData> readPixels() override;
    void configureFonts(const Font& font, float displayScale, Gdiplus::StringFormat * format);

    void * lockMemory(bool write_access = false) override {
      if (bitmap.get()) {
	Gdiplus::Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
	bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead | (write_access ? Gdiplus::ImageLockModeWrite : 0), getPixelFormat(getNumChannels()), &data);
	return data.Scan0;
      } else {
	return 0;
      }
    }
      
    void releaseMemory() override {
      if (bitmap) bitmap->UnlockBits(&data);
    }

    bool initializeContext() {
      if (!g) {
	if (!bitmap) {
	  bitmap = std::make_unique<Gdiplus::Bitmap>(16, 16, getPixelFormat(4));
	}
	g = std::make_unique<Gdiplus::Graphics>(bitmap.get());
	if (g) {
	  // Integer coordinates are pixel edges, as in HTML5 canvas.
	  g->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	  g->SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	  g->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	}
      }
      return g.get() != nullptr;
    }
    void drawNativeSurface(GDIPlusSurface & img, const Point & p, double w, double h, float displayScale, float globalAlpha, bool imageSmoothingEnabled);

  private:
    std::unique_ptr<Gdiplus::Bitmap> bitmap;
    std::unique_ptr<Gdiplus::Graphics> g;
    Gdiplus::BitmapData data;     
    BYTE * storage = 0;
  };
  
  class ContextGDIPlus : public Context {
  public:
    ContextGDIPlus(unsigned int _width, unsigned int _height, unsigned int _num_channels = 4, float _display_scale = 1.0f)
      : Context(_display_scale),
	default_surface(_width, _height, (unsigned int)(_display_scale * _width), (unsigned int)(_display_scale * _height), _num_channels)
    {
    
    }

    static void initialize() {
      if (!is_initialized) {
	// Initialize GDI+
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
	is_initialized = true;
      }
    }

    std::unique_ptr<Surface> createSurface(const ImageData & image) override {
      return std::unique_ptr<Surface>(new GDIPlusSurface(image));
    }
    std::unique_ptr<Surface> createSurface(unsigned int _width, unsigned int _height, unsigned int num_channels) override {
      return std::unique_ptr<Surface>(new GDIPlusSurface(_width, _height, (unsigned int)(_width * getDisplayScale()), (unsigned int)(_height * getDisplayScale()), num_channels));
    }

    GDIPlusSurface & getDefaultSurface() override { return default_surface; }
    const GDIPlusSurface & getDefaultSurface() const override { return default_surface; }

  private:   
    GDIPlusSurface default_surface;
    static bool is_initialized;
    static ULONG_PTR m_gdiplusToken;
  };

  class GDIPlusContextFactory : public ContextFactory  {
  public:
    // The converter turns an image name into a path; without one the name is the path.
    GDIPlusContextFactory(float display_scale, FilenameConverter * _converter = nullptr) : ContextFactory(display_scale), converter(_converter) {
      ContextGDIPlus::initialize();
    }
    std::unique_ptr<Context> createContext(unsigned int width, unsigned int height, unsigned int num_channels = 4) override {
      return std::make_unique<ContextGDIPlus>(width, height, num_channels, getDisplayScale());
    }
    std::unique_ptr<Surface> createSurface(unsigned int width, unsigned int height, unsigned int num_channels = 4) override {
      unsigned int aw = width * getDisplayScale(), ah = height * getDisplayScale();
      return std::make_unique<GDIPlusSurface>(width, height, aw, ah, num_channels);
    }
    std::unique_ptr<Image> loadImage(const std::string & filename) override;
    std::unique_ptr<Image> createImage() override;
    std::unique_ptr<Image> createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) override;

  private:
    FilenameConverter * converter;
  };
};

#endif
