#include <ImageData.h>

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
  auto target_size = calculateSize(target_width, target_height, num_channels_);

  std::unique_ptr<unsigned char[]> output_data(new unsigned char[target_size]);

  stbir_resize_uint8(data_.get(), getWidth(), getHeight(), 0, output_data.get(), target_width, target_height, 0, num_channels_);

  auto r = make_unique<ImageData>(output_data.get(), target_width, target_height, num_channels_);
  r->setFormat(order_, premultiplied_);
  return r;
}

std::unique_ptr<ImageData>
ImageData::colorize(const Color & color) const {
  assert(num_channels_ == 1);

  int red = int(255 * color.red * color.alpha);
  int green = int(255 * color.green * color.alpha);
  int blue = int(255 * color.blue * color.alpha);
  int alpha = int(255 * color.alpha);

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
static void blur_line(const unsigned char * src, unsigned char * dst, unsigned int count, size_t stride, unsigned int channels, const vector<int> & kernel) {
  int r = int(kernel.size() / 2);
  int total = 0;
  for (auto w : kernel) total += w;
  for (int i = 0; i < int(count); i++) {
    for (unsigned int c = 0; c < channels; c++) {
      int sum = 0;
      for (int k = -r; k <= r; k++) {
        int j = i + k;
        if (j >= 0 && j < int(count)) sum += src[j * stride + c] * kernel[k + r];
      }
      dst[i * stride + c] = (unsigned char)(sum / total);
    }
  }
}

std::unique_ptr<ImageData>
ImageData::blur(float hradius, float vradius) const {
  auto r = make_unique<ImageData>(width_, height_, num_channels_);
  r->setFormat(order_, premultiplied_);
  if (!isValid()) return r;

  unsigned int ch = num_channels_;
  size_t size = calculateSize();
  auto tmp = std::make_unique<unsigned char[]>(size);
  if (hradius > 0.0f) {
    auto kernel = make_kernel(hradius);
    for (unsigned int row = 0; row < height_; row++) {
      blur_line(getData() + size_t(row) * width_ * ch, tmp.get() + size_t(row) * width_ * ch, width_, ch, ch, kernel);
    }
  } else {
    memcpy(tmp.get(), getData(), size);
  }
  if (vradius > 0.0f) {
    auto kernel = make_kernel(vradius);
    for (unsigned int col = 0; col < width_; col++) {
      blur_line(tmp.get() + col * ch, r->getData() + col * ch, height_, width_ * ch, ch, kernel);
    }
  } else {
    memcpy(r->getData(), tmp.get(), size);
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
  order_ = order;
  if (num_channels_ == 4) premultiplied_ = premultiplied;
}
