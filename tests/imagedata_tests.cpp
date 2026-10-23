// Backend-independent ImageData tests.
#include <canvas/Image.h>
#include <canvas/Dither.h>
#include <canvas/Half.h>
#include <canvas/ImageData.h>
#include <canvas/Palette.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static const unsigned char * px(const ImageData & d, unsigned int x, unsigned int y) {
  return d.getData() + (y * d.getWidth() + x) * d.getNumChannels();
}

int main() {
  {  // A uniform opaque image stays uniform away from the edges.
    ImageData img(32, 32, 4);
    for (unsigned int i = 0; i < 32 * 32; i++) {
      unsigned char * p = img.getData() + 4 * i;
      p[0] = 40; p[1] = 80; p[2] = 120; p[3] = 255;
    }
    auto b = img.blur(3, 3);
    const unsigned char * p = px(*b, 16, 16);
    CHECK(std::abs(p[0] - 40) <= 1 && std::abs(p[1] - 80) <= 1 && std::abs(p[2] - 120) <= 1 && p[3] >= 254);
  }
  {  // Outside the image is transparent: a uniform image fades at its edge.
    ImageData img(32, 32, 4);
    for (unsigned int i = 0; i < 32 * 32; i++) {
      unsigned char * p = img.getData() + 4 * i;
      p[0] = p[1] = p[2] = p[3] = 255;
    }
    auto b = img.blur(3, 0);
    CHECK(px(*b, 0, 16)[3] < px(*b, 16, 16)[3]);
    CHECK(px(*b, 0, 16)[3] > 0);
  }
  {  // The kernel is symmetric: an impulse blurs to a symmetric bump, in
     // both directions.
    ImageData img(33, 33, 1);
    img.getData()[16 * 33 + 16] = 255;
    auto b = img.blur(4, 4);
    for (unsigned int k = 1; k <= 6; k++) {
      CHECK(px(*b, 16 - k, 16)[0] == px(*b, 16 + k, 16)[0]);
      CHECK(px(*b, 16, 16 - k)[0] == px(*b, 16, 16 + k)[0]);
    }
    CHECK(px(*b, 16, 16)[0] > px(*b, 18, 16)[0]);
  }
  {  // Premultiplied data stays valid: no colour channel exceeds alpha.
    ImageData img(16, 16, 4);
    for (unsigned int y = 0; y < 16; y++)
      for (unsigned int x = 0; x < 16; x++) {
        unsigned char * p = img.getData() + (y * 16 + x) * 4;
        if (x < 8) { p[0] = 0; p[1] = 0; p[2] = 255; p[3] = 255; }
      }
    auto b = img.blur(2, 2);
    for (unsigned int i = 0; i < 16 * 16; i++) {
      const unsigned char * p = b->getData() + 4 * i;
      CHECK(p[0] <= p[3] && p[1] <= p[3] && p[2] <= p[3]);
    }
  }
  {  // Straight RGBA converts to premultiplied BGRA and back (to within rounding).
    unsigned char rgba[4] = { 200, 100, 50, 128 };
    ImageData img(rgba, 1, 1, 4);
    CHECK(img.getChannelOrder() == ChannelOrder::RGB && !img.isPremultiplied());
    img.convertTo(ChannelOrder::BGR, true);
    CHECK(img.isPremultiplied() && img.getChannelOrder() == ChannelOrder::BGR);
    const unsigned char * p = img.getData();
    CHECK(p[0] == 25 && p[1] == 50 && p[2] == 100 && p[3] == 128);
    img.convertTo(ChannelOrder::RGB, false);
    CHECK(std::abs(p[0] - 200) <= 2 && std::abs(p[1] - 100) <= 2 && std::abs(p[2] - 50) <= 2 && p[3] == 128);
  }
  {  // A transparent pixel unpremultiplies to black, not a division by zero.
    unsigned char px4[4] = { 0, 0, 0, 0 };
    ImageData img(px4, 1, 1, 4);
    img.setFormat(ChannelOrder::BGR, true);
    img.convertTo(ChannelOrder::RGB, false);
    CHECK(img.getData()[0] == 0 && img.getData()[3] == 0);
  }
  {  // multiplyAlpha() only touches straight 4 channel data, and the format follows copies.
    unsigned char rgb[3] = { 10, 20, 30 };
    ImageData three(rgb, 1, 1, 3);
    three.multiplyAlpha();
    CHECK(three.getData()[0] == 10 && three.getData()[2] == 30);
    unsigned char rgba[4] = { 255, 255, 255, 128 };
    ImageData img(rgba, 1, 1, 4);
    img.multiplyAlpha();
    CHECK(img.isPremultiplied() && img.getData()[0] == 128);
    img.multiplyAlpha();  // already premultiplied
    CHECK(img.getData()[0] == 128);
    ImageData copy(img);
    CHECK(copy.isPremultiplied());
    CHECK(img.blur(1.0f, 1.0f)->isPremultiplied());
  }
  {  // Sizes don't wrap at 32 bits.
    CHECK(ImageData::calculateSize(70000, 70000, 4) == size_t(70000) * 70000 * 4);
  }
  {  // JPEG: JFIF, Exif and plain markers are all JPEG.
    const unsigned char jfif[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0, 16, 'J', 'F', 'I', 'F', 0, 1 };
    const unsigned char exif[] = { 0xFF, 0xD8, 0xFF, 0xE1, 0, 0, 'E', 'x', 'i', 'f', 0, 0 };
    const unsigned char dqt[] = { 0xFF, 0xD8, 0xFF, 0xDB };
    const unsigned char png[] = { 0x89, 'P', 'N', 'G' };
    CHECK(Image::isJPEG(jfif, sizeof jfif) && Image::isJPEG(exif, sizeof exif) && Image::isJPEG(dqt, sizeof dqt));
    CHECK(!Image::isJPEG(png, sizeof png) && !Image::isJPEG(dqt, 2));
  }
  {  // Colour strings.
    auto is = [](const std::string & text, float r, float g, float b, float a) {
      Color c(text);
      return std::abs(c.red - r) < 0.003f && std::abs(c.green - g) < 0.003f && std::abs(c.blue - b) < 0.003f && std::abs(c.alpha - a) < 0.003f;
    };
    CHECK(is("#f00", 1, 0, 0, 1) && is("#ff0000", 1, 0, 0, 1) && is("#FF0000", 1, 0, 0, 1));
    CHECK(is("#f008", 1, 0, 0, 8 * 17 / 255.0f) && is("#00ff0080", 0, 1, 0, 128 / 255.0f));
    CHECK(is("rgb(255, 128, 0)", 1, 128 / 255.0f, 0, 1));
    CHECK(is("rgba(0, 0, 255, 0.5)", 0, 0, 1, 0.5f) && is("rgba(0,0,255,.25)", 0, 0, 1, 0.25f));
    CHECK(is("rgb(100%, 0%, 50%)", 1, 0, 0.5f, 1) && is("rgba(0 0 0 / 50%)", 0, 0, 0, 0.5f));
    CHECK(is("rgba(300, -5, 0, 2)", 1, 0, 0, 1));
    CHECK(is("red", 1, 0, 0, 1) && is("  Tomato ", 1, 99 / 255.0f, 71 / 255.0f, 1));
    CHECK(is("rebeccapurple", 0x66 / 255.0f, 0x33 / 255.0f, 0x99 / 255.0f, 1) && is("yellowgreen", 0x9a / 255.0f, 0xcd / 255.0f, 0x32 / 255.0f, 1));
    CHECK(is("white", 1, 1, 1, 1) && is("black", 0, 0, 0, 1) && is("transparent", 0, 0, 0, 0));
    CHECK(is("ff0000", 1, 0, 0, 1));
    // Unknown strings are black, and don't assert.
    CHECK(is("", 0, 0, 0, 1) && is("nonsense", 0, 0, 0, 1) && is("rgba(1,2", 0, 0, 0, 1) && is("#12", 0, 0, 0, 1) && is("#gg0000", 0, 0, 0, 1));
    Color c;
    c = std::string("rgba(255, 0, 0, 0.5)");
    CHECK(c.red == 1.0f && c.alpha == 0.5f);
  }
  {  // Float ImageData conversions don't clamp; scaling and blur keep the range.
    float px[8] = { 5.0f, 0.0f, 1.0f, 0.5f,   2.0f, 2.0f, 2.0f, 0.0f };
    ImageData img(px, 2, 1, 4, PixelType::FLOAT32);
    img.convertTo(ChannelOrder::BGR, true);
    const float * d = img.getFloatData();
    CHECK(d[0] == 0.5f && d[1] == 0.0f && d[2] == 2.5f && d[3] == 0.5f);
    CHECK(d[4] == 0.0f && d[7] == 0.0f);
    img.convertTo(ChannelOrder::RGB, false);
    CHECK(near(img.getFloatData()[0], 5.0, 1e-6) && near(img.getFloatData()[2], 1.0, 1e-6));
    ImageData flat(8, 8, 4, PixelType::FLOAT32);
    for (size_t i = 0; i < 64; i++) { flat.getFloatData()[4 * i] = 20.0f; flat.getFloatData()[4 * i + 3] = 1.0f; }
    auto big = flat.scale(16, 16);
    CHECK(big->isFloat() && near(big->getFloatData()[4 * (8 * 16 + 8)], 20.0, 1e-3));
    auto blurred = flat.blur(2, 2);
    CHECK(blurred->isFloat() && near(blurred->getFloatData()[4 * (4 * 8 + 4)], 20.0, 0.5));
  }
  {  // colorize() of a mask: bytes, clamped, or floats when the colour is above 1.
    ImageData mask(2, 1, 1);
    mask.getData()[0] = 255;
    mask.getData()[1] = 128;
    auto hdr = mask.colorize(Color(4.0f, 2.0f, 0.5f, 0.5f));
    CHECK(hdr->isFloat() && hdr->isPremultiplied() && hdr->getNumChannels() == 4);
    CHECK(near(hdr->getFloatData()[0], 2.0, 1e-6) && near(hdr->getFloatData()[1], 1.0, 1e-6) && near(hdr->getFloatData()[3], 0.5, 1e-6));
    CHECK(near(hdr->getFloatData()[4], 2.0 * 128 / 255, 1e-5));
    auto sdr = mask.colorize(Color(1.0f, 0.5f, 0.0f, 1.0f));
    CHECK(!sdr->isFloat() && sdr->getData()[0] == 255 && sdr->getData()[1] == 127 && sdr->getData()[3] == 255);
  }
  {  // FP16 conversion, for backends whose float surface is half.
    float values[] = { 0.0f, 1.0f, -2.0f, 0.5f, 4.0f, 65504.0f, 0.000061035156f, 0.001f, 3.14159f, 1000.0f };
    for (float v : values) {
      float back = halfToFloat(floatToHalf(v));
      CHECK(near(back, v, std::fabs(v) * 1e-3 + 1e-7));
    }
    CHECK(floatToHalf(1.0f) == 0x3c00 && floatToHalf(-2.0f) == 0xc000 && halfToFloat(0x3555) > 0.333f && halfToFloat(0x3555) < 0.334f);
    CHECK(std::isinf(halfToFloat(floatToHalf(1e6f))) && floatToHalf(1e-9f) == 0);
  }
  {  // quantize() takes float images, clamped to [0, 1].
    float px[4 * 2] = { 5.0f, 5.0f, 5.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    ImageData img(px, 2, 1, 4, PixelType::FLOAT32);
    auto indices = quantize(img, Palette::xterm256());
    CHECK(indices.size() == 2 && indices[0] != indices[1]);
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
