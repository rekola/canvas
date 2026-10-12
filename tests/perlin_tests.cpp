// Backend-independent PerlinSurface tests.
#include <Context.h>
#include <Image.h>
#include <PerlinSurface.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static std::unique_ptr<Image> noise(unsigned w, unsigned h, int octaves = 1, float period = 32.0f) {
  PerlinSurface surface(w, h, w, h, octaves, 2, 2, period);
  return surface.createImage(1.0f);
}

// The mean difference between neighbouring pixels: how rough the noise is.
static double roughness(const ImageData & d) {
  double sum = 0;
  int n = 0;
  for (unsigned y = 0; y < d.getHeight(); y++) {
    for (unsigned x = 0; x + 1 < d.getWidth(); x++) {
      sum += std::abs(d.getData()[4 * (y * d.getWidth() + x)] - d.getData()[4 * (y * d.getWidth() + x + 1)]);
      n++;
    }
  }
  return sum / n;
}

int main() {
  {  // The image is grey, opaque and not flat.
    auto image = noise(64, 64);
    const ImageData & d = image->getData();
    CHECK(d.getWidth() == 64 && d.getHeight() == 64 && d.getNumChannels() == 4);
    std::set<int> values;
    int lo = 255, hi = 0;
    bool grey = true, opaque = true;
    for (unsigned i = 0; i < 64 * 64; i++) {
      const unsigned char * p = d.getData() + 4 * i;
      if (p[0] != p[1] || p[1] != p[2]) grey = false;
      if (p[3] != 255) opaque = false;
      values.insert(p[0]);
      lo = std::min<int>(lo, p[0]);
      hi = std::max<int>(hi, p[0]);
    }
    CHECK(grey && opaque);
    CHECK(values.size() > 30);
    CHECK(hi - lo > 60);
  }
  {  // The same parameters give the same pixels; other parameters other pixels.
    auto a = noise(32, 32), b = noise(32, 32), c = noise(32, 32, 1, 16.0f);
    CHECK(memcmp(a->getData().getData(), b->getData().getData(), 32 * 32 * 4) == 0);
    CHECK(memcmp(a->getData().getData(), c->getData().getData(), 32 * 32 * 4) != 0);
  }
  {  // A longer period is smoother; more octaves add fine detail.
    CHECK(roughness(noise(64, 64, 1, 64.0f)->getData()) < roughness(noise(64, 64, 1, 8.0f)->getData()));
    CHECK(roughness(noise(64, 64, 4, 32.0f)->getData()) > roughness(noise(64, 64, 1, 32.0f)->getData()));
  }
  {  // More octaves don't clip the noise to white or black.
    auto image = noise(64, 64, 6);
    int extreme = 0;
    for (unsigned i = 0; i < 64 * 64; i++) {
      unsigned char v = image->getData().getData()[4 * i];
      if (v == 0 || v == 255) extreme++;
    }
    CHECK(extreme < 64 * 64 / 10);
  }
  {  // It is a source, not a target: drawing does nothing and measuring gives nothing.
    PerlinSurface surface(16, 16, 16, 16);
    auto before = surface.createImage(1.0f);
    Style style(nullptr);
    style = Color(1.0f, 0.0f, 0.0f, 1.0f);
    Path2D path;
    path.rect(0, 0, 16, 16);
    surface.renderPath(RenderMode::FILL, TransformationMatrix(), path, style, 1.0f, Operator::SOURCE_OVER, 1.0f, 1.0f, 0, 0, 0, Color(), Path2D(), LineStyle());
    surface.renderText(RenderMode::FILL, Font(nullptr), style, TextBaseline::TOP, TextAlign::LEFT, "text", Point(0, 0), 1.0f, Operator::SOURCE_OVER, 1.0f, 1.0f, 0, 0, 0, Color(), Path2D());
    ImageData red(16, 16, 4);
    surface.drawImage(red, Point(0, 0), 16, 16, 1.0f, 1.0f, 0, 0, 0, Color(), Path2D());
    auto after = surface.createImage(1.0f);
    CHECK(memcmp(before->getData().getData(), after->getData().getData(), 16 * 16 * 4) == 0);
    CHECK(surface.measureText(Font(nullptr), "text", TextBaseline::TOP, 1.0f).width == 0);
  }
  {  // Blurring and colourizing, which read the surface's memory, work on it.
    PerlinSurface surface(16, 16, 16, 16, 2);
    auto blurred = surface.blur(2.0f, 2.0f);
    CHECK(blurred && blurred->getWidth() == 16 && blurred->getNumChannels() == 4);
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
