#ifndef _CANVAS_IMAGE_H_
#define _CANVAS_IMAGE_H_

#include "ImageData.h"
#ifdef CANVAS_HAS_TEXTURE_PACKING
#include "PackedImageData.h"
#endif

#include <string>

namespace canvas {
  class Image {
  public:
    Image(float _display_scale) : display_scale(_display_scale) { }
    Image(const char * _filename, float _display_scale)
      : filename(_filename), display_scale(_display_scale) { }
    Image(const std::string & _filename, float _display_scale)
      : filename(_filename), display_scale(_display_scale) { }
    Image(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels, float _display_scale)
      : data(new ImageData(_data, _width, _height, _num_channels)),
        display_scale(_display_scale) { }

    virtual ~Image() = default;

    void decode(const void * buffer, size_t size);
    // The target size is logical (it may be fractional, as a surface's
    // logical size can be); the pixels are rounded down as for surfaces.
    void scale(float target_width, float target_height) {
      if (!filename.empty() && !data.get()) {
	loadFile();
      }
      if (data.get()) {
	data = data->scale(toPixels(target_width), toPixels(target_height));
      }
    }

    const ImageData & getData() {
      if (!filename.empty() && !data.get()) {
	loadFile();
      }
      if (data.get()) {
	return *data;
      } else {
	return null_image;
      }
    }

    std::unique_ptr<ImageData> & getDataPtr() { return data; }
    std::unique_ptr<ImageData> && moveDataPtr() { return std::move(data); }
    
    const std::string & getFilename() const { return filename; }

    void setDisplayScale(float f) { display_scale = f; }
    float getDisplayScale() const { return display_scale; }

#ifdef CANVAS_HAS_TEXTURE_PACKING
    std::unique_ptr<PackedImageData> pack(InternalFormat format, int num_levels) const {
      return std::unique_ptr<PackedImageData>(new PackedImageData(format, num_levels, *data));
    }
#endif
    
    static bool isPNG(const unsigned char * buffer, size_t size);
    static bool isJPEG(const unsigned char * buffer, size_t size);
    static bool isGIF(const unsigned char * buffer, size_t size);
    static bool isBMP(const unsigned char * buffer, size_t size);
    static bool isXML(const unsigned char * buffer, size_t size);

  protected:
    // Matches canvas::toPixels() in Surface.h.
    unsigned int toPixels(float logical) const {
      double v = double(logical) * display_scale;
      return v > 0.0 ? (unsigned int)(v + 1e-4) : 0;
    }

    static std::unique_ptr<ImageData> loadFromMemory(const void * buffer, size_t size);
    static std::unique_ptr<ImageData> loadFromFile(const std::string & filename);
    virtual void loadFile() { }
    
    std::string filename;
    std::unique_ptr<ImageData> data;

  private:
    float display_scale;

    static inline ImageData null_image;
  };
};
#endif
