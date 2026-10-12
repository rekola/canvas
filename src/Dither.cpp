#include <Dither.h>

#include <ImageData.h>

#include <algorithm>
#include <cmath>

using namespace canvas;

namespace {
  // The pixel as 0-255 channels, composited over the background.
  struct Rgb { int r, g, b; };

  Rgb readPixel(const ImageData & image, size_t i, const DitherOptions & options) {
    unsigned int channels = image.getNumChannels();
    const unsigned char * p = image.getData() + channels * i;
    bool bgr = image.getChannelOrder() == ChannelOrder::BGR;
    int bg_r = options.backgroundRed, bg_g = options.backgroundGreen, bg_b = options.backgroundBlue;
    if (channels == 1) return Rgb{ p[0], p[0], p[0] };
    if (channels == 2) {
      int a = p[1];
      int v = (p[0] * a + 127) / 255;
      return Rgb{ v + (bg_r * (255 - a) + 127) / 255, v + (bg_g * (255 - a) + 127) / 255, v + (bg_b * (255 - a) + 127) / 255 };
    }
    int r = bgr ? p[2] : p[0], g = p[1], b = bgr ? p[0] : p[2];
    if (channels == 3) return Rgb{ r, g, b };
    int a = p[3];
    if (!image.isPremultiplied()) {
      r = (r * a + 127) / 255;
      g = (g * a + 127) / 255;
      b = (b * a + 127) / 255;
    }
    // Premultiplied over the background: colour + background * (1 - alpha).
    return Rgb{ std::min(255, r + (bg_r * (255 - a) + 127) / 255),
		std::min(255, g + (bg_g * (255 - a) + 127) / 255),
		std::min(255, b + (bg_b * (255 - a) + 127) / 255) };
  }

  inline int clamp255(double v) { return v < 0 ? 0 : v > 255 ? 255 : (int)std::lround(v); }

  // The Bayer matrix of side n (a power of two): thresholds 0 .. n*n-1.
  std::vector<int> bayer(int n) {
    std::vector<int> m(1, 0);
    for (int size = 1; size < n; size *= 2) {
      int next = size * 2;
      std::vector<int> bigger(next * next);
      for (int y = 0; y < size; y++) {
	for (int x = 0; x < size; x++) {
	  int v = 4 * m[y * size + x];
	  bigger[y * next + x] = v;
	  bigger[y * next + x + size] = v + 2;
	  bigger[(y + size) * next + x] = v + 3;
	  bigger[(y + size) * next + x + size] = v + 1;
	}
      }
      m.swap(bigger);
    }
    return m;
  }
}

std::vector<unsigned char>
canvas::quantize(const ImageData & image, const Palette & palette, const DitherOptions & options) {
  size_t width = image.getWidth(), height = image.getHeight();
  std::vector<unsigned char> out(width * height, 0);
  if (palette.empty() || !image.isValid()) return out;

  if (options.method == DitherMethod::FLOYD_STEINBERG) {
    // Error rows with a border on both sides; the error is carried in floats.
    std::vector<float> current((width + 2) * 3, 0.0f), next((width + 2) * 3, 0.0f);
    for (size_t y = 0; y < height; y++) {
      std::fill(next.begin(), next.end(), 0.0f);
      for (size_t x = 0; x < width; x++) {
	Rgb p = readPixel(image, y * width + x, options);
	float want[3] = { p.r + current[(x + 1) * 3 + 0], p.g + current[(x + 1) * 3 + 1], p.b + current[(x + 1) * 3 + 2] };
	int r = clamp255(want[0]), g = clamp255(want[1]), b = clamp255(want[2]);
	unsigned int index = palette.nearest(r, g, b, options.firstIndex);
	out[y * width + x] = (unsigned char)index;
	float error[3] = { want[0] - palette[index].red, want[1] - palette[index].green, want[2] - palette[index].blue };
	for (int c = 0; c < 3; c++) {
	  current[(x + 2) * 3 + c] += error[c] * 7.0f / 16.0f;
	  next[x * 3 + c] += error[c] * 3.0f / 16.0f;
	  next[(x + 1) * 3 + c] += error[c] * 5.0f / 16.0f;
	  next[(x + 2) * 3 + c] += error[c] * 1.0f / 16.0f;
	}
      }
      current.swap(next);
    }
  } else if (options.method == DitherMethod::ORDERED) {
    int n = options.matrixSize;
    if (n != 2 && n != 4 && n != 8 && n != 16) n = 8;
    std::vector<int> matrix = bayer(n);
    double strength = options.strength > 0 ? options.strength : palette.spacing();
    double cells = double(n) * n;
    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
	Rgb p = readPixel(image, y * width + x, options);
	// A threshold in (-strength / 2, strength / 2), the same for all channels.
	double bias = ((matrix[(y % n) * n + (x % n)] + 0.5) / cells - 0.5) * strength;
	out[y * width + x] = (unsigned char)palette.nearest(clamp255(p.r + bias), clamp255(p.g + bias), clamp255(p.b + bias), options.firstIndex);
      }
    }
  } else {
    for (size_t i = 0; i < width * height; i++) {
      Rgb p = readPixel(image, i, options);
      out[i] = (unsigned char)palette.nearest(p.r, p.g, p.b, options.firstIndex);
    }
  }
  return out;
}
