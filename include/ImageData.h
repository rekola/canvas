#ifndef _CANVAS_IMAGEDATA_H_
#define _CANVAS_IMAGEDATA_H_

#include <Color.h>

#include <cstring>
#include <memory>

namespace canvas {
  // Byte order of the colour channels in 3 and 4 channel data (alpha is
  // last). A 1 channel image is a single value per pixel: the alpha of a
  // shadow mask, which is also how drawImage() treats it.
  enum class ChannelOrder { RGB, BGR };

  // The bytes are `width * height * channels`, rows top to bottom without
  // padding. Which colours they are is the format: RGB order with straight
  // (not premultiplied) alpha unless set otherwise, which is what image
  // files decode to. Surface::createImage() sets BGR order and
  // premultiplied alpha, the native layout of the Cairo backend.
  class ImageData {
  public:
    ImageData() : width_(0), height_(0), num_channels_(0) { }
    ImageData(const unsigned char * data, unsigned int width, unsigned int height, unsigned short num_channels)
      : width_(width), height_(height), num_channels_(num_channels)
    {
      auto s = calculateSize();
      data_ = std::unique_ptr<unsigned char[]>(new unsigned char[s]);
      if (data) {
	memcpy(data_.get(), data, s);
      } else {
	memset(data_.get(), 0, s);
      }
    }
    ImageData(unsigned int width, unsigned int height, unsigned short num_channels)
      : width_(width), height_(height), num_channels_(num_channels) {
      auto s = calculateSize();
      data_ = std::unique_ptr<unsigned char[]>(new unsigned char[s]);
      memset(data_.get(), 0, s);  
    }
    
    ImageData(const ImageData & other)
      : width_(other.getWidth()), height_(other.getHeight()), num_channels_(other.num_channels_),
        order_(other.order_), premultiplied_(other.premultiplied_)
    {
      auto s = calculateSize();
      data_ = std::unique_ptr<unsigned char[]>(new unsigned char[s]);
      if (other.getData()) {
	memcpy(data_.get(), other.getData(), s);
      } else {
	memset(data_.get(), 0, s);
      }
    }

    ImageData & operator=(const ImageData & other) {
      if (this != &other) {
	width_ = other.width_;
	height_ = other.height_;
	num_channels_ = other.num_channels_;
	order_ = other.order_;
	premultiplied_ = other.premultiplied_;
	auto s = calculateSize();
	data_ = std::unique_ptr<unsigned char[]>(new unsigned char[s]);
	if (other.getData()) {
	  memcpy(data_.get(), other.getData(), s);
	} else {
	  memset(data_.get(), 0, s);
	}	
      }
      return *this;
    }

    ImageData(ImageData && other) = default;
    ImageData & operator=(ImageData && other) = default;
    
    std::unique_ptr<ImageData> scale(unsigned int target_width, unsigned int target_height) const;
    std::unique_ptr<ImageData> colorize(const Color & color) const;
    std::unique_ptr<ImageData> blur(float hradius, float vradius) const;

    // Premultiplies straight 4 channel data; other data is left alone.
    void multiplyAlpha();

    ChannelOrder getChannelOrder() const { return order_; }
    bool isPremultiplied() const { return premultiplied_; }
    // Says what the bytes are, without changing them.
    ImageData & setFormat(ChannelOrder order, bool premultiplied) {
      order_ = order;
      premultiplied_ = premultiplied;
      return *this;
    }
    // Changes the bytes to the given format. Only 3 and 4 channel data has
    // colour channels to reorder, and only 4 channel data has alpha.
    void convertTo(ChannelOrder order, bool premultiplied);
    
    bool isValid() const { return width_ != 0 && height_ != 0 && num_channels_ != 0; }
    unsigned int getWidth() const { return width_; }
    unsigned int getHeight() const { return height_; }
    unsigned short getNumChannels() const { return num_channels_; }

    unsigned char * getData() { return data_.get(); }
    const unsigned char * getData() const { return data_.get(); }

    size_t getBytesPerRow() const { return size_t(num_channels_) * width_; }
    
    static size_t calculateSize(unsigned int width, unsigned int height, unsigned short num_channels) { return size_t(width) * height * num_channels; }
    size_t calculateSize() const { return calculateSize(width_, height_, num_channels_); }
    
  private:
    unsigned int width_, height_;
    unsigned short num_channels_;
    ChannelOrder order_ = ChannelOrder::RGB;
    bool premultiplied_ = false;
    std::unique_ptr<unsigned char[]> data_;
  };
};
#endif
