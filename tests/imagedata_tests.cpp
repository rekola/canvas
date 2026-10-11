// Backend-independent ImageData tests.
#include <Image.h>
#include <ImageData.h>

#include <cstdio>
#include <cstdlib>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const unsigned char * px(const ImageData & d, unsigned x, unsigned y) {
  return d.getData() + (y * d.getWidth() + x) * d.getNumChannels();
}

int main() {
  {  // A uniform opaque image stays uniform away from the edges.
    ImageData img(32, 32, 4);
    for (unsigned i = 0; i < 32 * 32; i++) {
      unsigned char * p = img.getData() + 4 * i;
      p[0] = 40; p[1] = 80; p[2] = 120; p[3] = 255;
    }
    auto b = img.blur(3, 3);
    const unsigned char * p = px(*b, 16, 16);
    CHECK(std::abs(p[0] - 40) <= 1 && std::abs(p[1] - 80) <= 1 && std::abs(p[2] - 120) <= 1 && p[3] >= 254);
  }
  {  // Outside the image is transparent: a uniform image fades at its edge.
    ImageData img(32, 32, 4);
    for (unsigned i = 0; i < 32 * 32; i++) {
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
    for (unsigned k = 1; k <= 6; k++) {
      CHECK(px(*b, 16 - k, 16)[0] == px(*b, 16 + k, 16)[0]);
      CHECK(px(*b, 16, 16 - k)[0] == px(*b, 16, 16 + k)[0]);
    }
    CHECK(px(*b, 16, 16)[0] > px(*b, 18, 16)[0]);
  }
  {  // Premultiplied data stays valid: no colour channel exceeds alpha.
    ImageData img(16, 16, 4);
    for (unsigned y = 0; y < 16; y++)
      for (unsigned x = 0; x < 16; x++) {
        unsigned char * p = img.getData() + (y * 16 + x) * 4;
        if (x < 8) { p[0] = 0; p[1] = 0; p[2] = 255; p[3] = 255; }
      }
    auto b = img.blur(2, 2);
    for (unsigned i = 0; i < 16 * 16; i++) {
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
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
