#include <canvas/ImageData.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <cassert>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

using namespace std;
using namespace canvas;

std::unique_ptr<ImageData>
ImageData::scale(unsigned int target_width, unsigned int target_height) const {
  auto r = make_unique<ImageData>(target_width, target_height, num_channels_, type_);
  r->setFormat(order_, premultiplied_);
  if (!isValid() || !target_width || !target_height) return r;

  if (isFloat()) {
    stbir_resize_float(getFloatData(), getWidth(), getHeight(), 0, r->getFloatData(), target_width, target_height, 0, num_channels_);
  } else {
    stbir_resize_uint8(data_.get(), getWidth(), getHeight(), 0, r->getData(), target_width, target_height, 0, num_channels_);
  }
  return r;
}

std::unique_ptr<ImageData>
ImageData::colorize(const Color & color) const {
  assert(num_channels_ == 1 && !isFloat());

  if (color.red > 1.0f || color.green > 1.0f || color.blue > 1.0f) {
    // Above 1 does not fit in a byte: premultiplied floats, red first.
    auto r = make_unique<ImageData>(width_, height_, 4, PixelType::FLOAT32);
    r->setFormat(ChannelOrder::RGB, true);
    float * out = r->getFloatData();
    for (size_t i = 0; i < size_t(width_) * height_; i++) {
      float cov = data_[i] / 255.0f * color.alpha;
      out[4 * i + 0] = color.red * cov;
      out[4 * i + 1] = color.green * cov;
      out[4 * i + 2] = color.blue * cov;
      out[4 * i + 3] = cov;
    }
    return r;
  }

  auto to_byte = [](float v) { return int(255 * std::min(1.0f, std::max(0.0f, v))); };
  int red = to_byte(color.red * color.alpha);
  int green = to_byte(color.green * color.alpha);
  int blue = to_byte(color.blue * color.alpha);
  int alpha = to_byte(color.alpha);

  // Premultiplied, with red first.
  auto r = make_unique<ImageData>(width_, height_, 4);
  r->setFormat(ChannelOrder::RGB, true);

  unsigned char * target_buffer = r->getData();
  for (size_t i = 0; i < size_t(width_) * height_; i++) {
    unsigned char v = data_[i];
    target_buffer[4 * i + 0] = (unsigned char)(red * v / 255);
    target_buffer[4 * i + 1] = (unsigned char)(green * v / 255);
    target_buffer[4 * i + 2] = (unsigned char)(blue * v / 255);
    target_buffer[4 * i + 3] = (unsigned char)(alpha * v / 255);
  }

  return r;
}

// A symmetric Gaussian, sigma = radius / 3, in integer weights.
static vector<int> make_kernel(float radius) {
  int r = (int)ceil(radius);
  float sigma = radius / 3;
  float sigma22 = 2.0f * sigma * sigma;
  vector<int> kernel;
  kernel.reserve(2 * r + 1);
  for (int k = -r; k <= r; k++) {
    kernel.push_back(int(256.0f * exp(-float(k * k) / sigma22) + 0.5f));
  }
  return kernel;
}

// One separable pass along a line of `count` pixels `stride` bytes apart,
// `channels` interleaved bytes each. Pixels outside the image are
// transparent (zero) and the weights are not renormalized, so a shape
// near the edge fades out instead of keeping its brightness. The data may
// be premultiplied: a linear filter commutes with premultiplication.
// `stride` is in elements (bytes or floats).
template <typename T, typename Sum>
static void blur_line(const T * src, T * dst, unsigned int count, size_t stride, unsigned int channels, const vector<int> & kernel) {
  int r = int(kernel.size() / 2);
  int total = 0;
  for (auto w : kernel) total += w;
  for (int i = 0; i < int(count); i++) {
    for (unsigned int c = 0; c < channels; c++) {
      Sum sum = 0;
      for (int k = -r; k <= r; k++) {
        int j = i + k;
        if (j >= 0 && j < int(count)) sum += src[j * stride + c] * kernel[k + r];
      }
      dst[i * stride + c] = (T)(sum / total);
    }
  }
}

template <typename T, typename Sum>
static void blur_pixels(const T * in, T * out, unsigned int width, unsigned int height, unsigned int ch, float hradius, float vradius) {
  size_t count = size_t(width) * height * ch;
  vector<T> tmp(count);
  if (hradius > 0.0f) {
    auto kernel = make_kernel(hradius);
    for (unsigned int row = 0; row < height; row++) {
      blur_line<T, Sum>(in + size_t(row) * width * ch, tmp.data() + size_t(row) * width * ch, width, ch, ch, kernel);
    }
  } else {
    memcpy(tmp.data(), in, count * sizeof(T));
  }
  if (vradius > 0.0f) {
    auto kernel = make_kernel(vradius);
    for (unsigned int col = 0; col < width; col++) {
      blur_line<T, Sum>(tmp.data() + col * ch, out + col * ch, height, size_t(width) * ch, ch, kernel);
    }
  } else {
    memcpy(out, tmp.data(), count * sizeof(T));
  }
}

std::unique_ptr<ImageData>
ImageData::blur(float hradius, float vradius) const {
  auto r = make_unique<ImageData>(width_, height_, num_channels_, type_);
  r->setFormat(order_, premultiplied_);
  if (!isValid()) return r;

  if (isFloat()) {
    blur_pixels<float, double>(getFloatData(), r->getFloatData(), width_, height_, num_channels_, hradius, vradius);
  } else {
    blur_pixels<unsigned char, int>(getData(), r->getData(), width_, height_, num_channels_, hradius, vradius);
  }
  return r;
}

void
ImageData::multiplyAlpha() {
  if (num_channels_ != 4 || premultiplied_) return;
  convertTo(order_, true);
}

void
ImageData::convertTo(ChannelOrder order, bool premultiplied) {
  if (num_channels_ != 3 && num_channels_ != 4) return;
  bool swap = order != order_;
  bool change_alpha = num_channels_ == 4 && premultiplied != premultiplied_;
  size_t n = size_t(width_) * height_;
  if (isFloat()) {
    // Nothing is clamped or rounded: the numbers are the client's.
    float * p = getFloatData();
    for (size_t i = 0; i < n; i++, p += num_channels_) {
      if (change_alpha) {
	float a = p[3];
	for (int c = 0; c < 3; c++) p[c] = premultiplied ? p[c] * a : (a > 0.0f ? p[c] / a : 0.0f);
      }
      if (swap) std::swap(p[0], p[2]);
    }
  } else {
    unsigned char * p = data_.get();
    for (size_t i = 0; i < n; i++, p += num_channels_) {
      if (change_alpha) {
	unsigned int a = p[3];
	for (int c = 0; c < 3; c++) {
	  if (premultiplied) p[c] = (unsigned char)((p[c] * a + 127) / 255);
	  else p[c] = a ? (unsigned char)std::min(255u, (p[c] * 255u + a / 2) / a) : 0;
	}
      }
      if (swap) std::swap(p[0], p[2]);
    }
  }
  order_ = order;
  if (num_channels_ == 4) premultiplied_ = premultiplied;
}
