#ifndef _CANVAS_SURFACE_H_
#define _CANVAS_SURFACE_H_

#include "Color.h"
#include "Path2D.h"
#include "Style.h"
#include "Font.h"
#include "TextBaseline.h"
#include "TextAlign.h"
#include "TextMetrics.h"
#include "Operator.h"
#include "ImageData.h"
#include "LineStyle.h"
#ifdef CANVAS_HAS_TEXTURE_PACKING
#include "PackedImageData.h"
#endif

#include <cassert>
#include <cstring>
#include <memory>

namespace canvas {
  class Context;
  class ImageData;
  class Image;

  enum class RenderMode {
    FILL = 1,
    STROKE
  };

  class Surface {
  public:
    friend class Context;

  Surface(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) :
    logical_width(_logical_width),
      logical_height(_logical_height),
      actual_width(_actual_width),
      actual_height(_actual_height),
      num_channels(_num_channels)
	{ }
    
    Surface(const Surface & other) = delete;
    Surface & operator=(const Surface & other) = delete;
    virtual ~Surface() = default;

    virtual void resize(unsigned int _logical_width, unsigned int _logical_height, unsigned int _actual_width, unsigned int _actual_height, unsigned int _num_channels) {
      logical_width = _logical_width;
      logical_height = _logical_height;
      actual_width = _actual_width;
      actual_height = _actual_height;
      num_channels = _num_channels;
    }

    virtual void renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, const LineStyle & lineStyle) = 0;
    virtual void renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) = 0;
    virtual TextMetrics measureText(const Font & font, const std::string & text, TextBaseline textBaseline, float displayScale) = 0;
	  
    virtual void drawImage(Surface & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) = 0;
    virtual void drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) = 0;
    virtual std::unique_ptr<Image> createImage(float display_scale) = 0;

#ifdef CANVAS_HAS_TEXTURE_PACKING
    std::unique_ptr<PackedImageData> createPackedImage() {
      unsigned char * buffer = (unsigned char *)lockMemory(false);
      assert(buffer);
      if (buffer) {
	auto image = std::make_unique<PackedImageData>(InternalFormat::RGBA8, getActualWidth(), getActualHeight(), 1, buffer);
	releaseMemory();
	return image;
      } else {
	return std::make_unique<PackedImageData>(InternalFormat::RGBA8, getActualWidth(), getActualHeight(), 1);
      }
    }
#endif

    std::unique_ptr<ImageData> blur(float hradius, float vradius) {
      return readPixels()->blur(hradius, vradius);
    }
    
    std::unique_ptr<ImageData> colorize(const Color & color) {
      return readPixels()->colorize(color);
    }

    unsigned int getLogicalWidth() const { return logical_width; }
    unsigned int getLogicalHeight() const { return logical_height; }
    unsigned int getActualWidth() const { return actual_width; }
    unsigned int getActualHeight() const { return actual_height; }
    unsigned int getNumChannels() const { return num_channels; }
    
  protected:
    // What lockMemory() points at: the bytes between the starts of two
    // rows (a backend may pad them), and what the bytes are. By default
    // unpadded and native: premultiplied, BGR order.
    virtual size_t getMemoryStride() const { return size_t(actual_width) * num_channels; }
    virtual ChannelOrder getMemoryChannelOrder() const { return ChannelOrder::BGR; }
    virtual bool isMemoryPremultiplied() const { return true; }

    // A copy of the pixels as an unpadded ImageData with its format set.
    virtual std::unique_ptr<ImageData> readPixels() {
      auto r = std::make_unique<ImageData>(actual_width, actual_height, num_channels);
      r->setFormat(getMemoryChannelOrder(), isMemoryPremultiplied());
      auto src = (const unsigned char *)lockMemory(false);
      if (src) {
	size_t row = r->getBytesPerRow(), stride = getMemoryStride();
	for (unsigned int y = 0; y < actual_height; y++) {
	  memcpy(r->getData() + y * row, src + y * stride, row);
	}
      }
      releaseMemory();
      return r;
    }

    virtual void * lockMemory(bool write_access = false) = 0;
    virtual void releaseMemory() = 0;

  private:
    unsigned int logical_width, logical_height, actual_width, actual_height, num_channels;
  };
};

#endif
