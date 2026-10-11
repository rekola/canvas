// Backend-independent ImageData tests.
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
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
