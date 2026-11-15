// ColorSpace::LINEAR_REC2020: an 8-bit surface is given what it can show of a
// linear Rec.2020 colour (gamut mapped, light past white compressed), a float
// surface the numbers as given, and the default changes nothing.
#if defined(CANVAS_HAS_CAIRO)
#include <canvas/ContextCairo.h>
namespace canvas { typedef ContextCairo TestContext; }
#elif defined(CANVAS_HAS_GDIPLUS)
#include <canvas/ContextGDIPlus.h>
namespace canvas { typedef ContextGDIPlus TestContext; }
#elif defined(CANVAS_HAS_QUARTZ)
#include <canvas/ContextQuartz2D.h>
namespace canvas {
  class TestContext : public ContextQuartz2D {
  public:
    TestContext(PixelSize size, unsigned int channels = 4, float scale = 1.0f, PixelType type = PixelType::UINT8)
      : ContextQuartz2D(std::make_shared<Quartz2DCache>(), size, channels, scale, type) { }
  };
}
#endif

#include <canvas/ColorSpace.h>
#include <canvas/Context.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

struct P { int r, g, b; };

// Backends turn a float colour into a byte their own way (GDI+ truncates where
// Cairo rounds), so a level is right to within one.
static bool is(const P & p, int r, int g, int b, const char * what, int line) {
  bool ok = std::abs(p.r - r) <= 1 && std::abs(p.g - g) <= 1 && std::abs(p.b - b) <= 1;
  if (!ok) std::printf("%s:%d: %s is %d, %d, %d, expected %d, %d, %d\n", __FILE__, line, what, p.r, p.g, p.b, r, g, b);
  return ok;
}
#define IS(p, r, g, b) do { if (!is(p, r, g, b, #p, __LINE__)) failures++; } while (0)

// GDI+ has no float surfaces, so its constructor takes no pixel type.
#if defined(CANVAS_HAS_GDIPLUS) && !defined(CANVAS_HAS_CAIRO)
#define CANVAS_TEST_NO_FLOAT 1
static TestContext make(unsigned int w, unsigned int h, PixelType) { return TestContext(PixelSize{ w, h }, 4, 1.0f); }
#else
static TestContext make(unsigned int w, unsigned int h, PixelType type) { return TestContext(PixelSize{ w, h }, 4, 1.0f, type); }
#endif

static P bytes(Context & ctx, int x, int y) {
  auto d = ctx.getDefaultSurface().getImageData();
  const unsigned char * p = d->getData() + (size_t(y) * d->getWidth() + size_t(x)) * d->getNumChannels();
  if (d->getChannelOrder() == ChannelOrder::BGR) return { p[2], p[1], p[0] };
  return { p[0], p[1], p[2] };
}

static P draw8(ColorSpace cs, const Color & c) {
  TestContext ctx = make(4, 4, PixelType::UINT8);
  ctx.setInputColorSpace(cs);
  ctx.fillStyle = c;
  ctx.fillRect(0, 0, 4, 4);
  return bytes(ctx, 1, 1);
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
#if defined(CANVAS_HAS_GDIPLUS) && !defined(CANVAS_HAS_CAIRO)
  ContextGDIPlus::initialize();
#endif
  {  // The default is as given: 8-bit output is unchanged.
    P p = draw8(ColorSpace::AS_GIVEN, Color(1.0f, 0.0f, 0.5f, 1.0f));
    IS(p, 255, 0, 128);
  }
  {  // White stays white; a mid grey is encoded (linear 0.214 is half in sRGB).
    P w = draw8(ColorSpace::LINEAR_REC2020, Color(1.0f, 1.0f, 1.0f, 1.0f));
    IS(w, 255, 255, 255);
    P g = draw8(ColorSpace::LINEAR_REC2020, Color(0.2140f, 0.2140f, 0.2140f, 1.0f));
    IS(g, 128, 128, 128);
    P k = draw8(ColorSpace::LINEAR_REC2020, Color(0.0f, 0.0f, 0.0f, 1.0f));
    IS(k, 0, 0, 0);
  }
  {  // Light past white is compressed and washes toward white: brighter is paler.
    P a = draw8(ColorSpace::LINEAR_REC2020, Color(1.2f, 0.1f, 0.1f, 1.0f));
    P b = draw8(ColorSpace::LINEAR_REC2020, Color(3.0f, 0.1f, 0.1f, 1.0f));
    P c = draw8(ColorSpace::LINEAR_REC2020, Color(50.0f, 0.1f, 0.1f, 1.0f));
    CHECK(a.r <= 255);
    IS(P({ b.r, 0, 0 }), 255, 0, 0);
    IS(P({ c.r, 0, 0 }), 255, 0, 0);
    CHECK(b.g > a.g && c.g > b.g && c.b > b.b);
  }
  {  // A colour outside sRGB is mapped in, not clipped to a corner.
    P p = draw8(ColorSpace::LINEAR_REC2020, Color(0.0f, 1.0f, 0.0f, 1.0f));
    // Pure Rec.2020 green has a negative red in sRGB; its chroma is reduced onto
    // the gamut edge (red 0) and its hue kept, where a clip would give 0, 255, 0.
    CHECK(p.r == 0 && p.g > 200 && p.g < 255 && p.b > 50);
  }
  {  // A gradient across white follows the map, not a line between its mapped ends.
    TestContext ctx = make(64, 4, PixelType::UINT8);
    ctx.setInputColorSpace(ColorSpace::LINEAR_REC2020);
    auto g = ctx.createLinearGradient(0, 0, 64, 0);
    g.addColorStop(0.0f, Color(0.2f, 0.2f, 0.2f, 1.0f));
    g.addColorStop(1.0f, Color(4.0f, 4.0f, 4.0f, 1.0f));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 64, 4);
    P mid = bytes(ctx, 32, 1);  // linear 2.1: white
    CHECK(mid.r >= 250 && mid.g >= 250 && mid.b >= 250);
    P low = bytes(ctx, 4, 1);
    CHECK(low.r < 200);
  }
#ifndef CANVAS_TEST_NO_FLOAT
  {  // The primaries of sRGB are on the gamut's edge, and how a build rounds the primaries conversion leaves a
     // component a hair either side of it; that is not out of gamut, so every level of a primary stays exact.
    // (A build with fused multiply-adds hides the noise; one without them shows it.)
    const double m[3][3] = { { 0.6274039, 0.3292830, 0.0433131 }, { 0.0690973, 0.9195404, 0.0113623 }, { 0.0163914, 0.0880133, 0.8955953 } };
    int bad = 0;
    for (int i = 0; i < 256; i++) {
      double lin = i <= 10 ? i / 255.0 / 12.92 : std::pow((i / 255.0 + 0.055) / 1.055, 2.4);
      for (int channel = 0; channel < 3; channel++) {
        float rgb[3];
        for (int k = 0; k < 3; k++) rgb[k] = static_cast<float>(m[k][channel] * lin);
        P p = draw8(ColorSpace::LINEAR_REC2020, Color(rgb[0], rgb[1], rgb[2], 1.0f));
        int expect[3] = { 0, 0, 0 };
        expect[channel] = i;
        if (std::abs(p.r - expect[0]) > 1 || std::abs(p.g - expect[1]) > 1 || std::abs(p.b - expect[2]) > 1) {
          if (bad < 3) std::printf("primary %d level %d: got %d, %d, %d\n", channel, i, p.r, p.g, p.b);
          bad++;
        }
      }
    }
    CHECK(bad == 0);
  }
  {  // A float surface keeps the numbers.
    TestContext ctx = make(4, 4, PixelType::FLOAT32);
    ctx.setInputColorSpace(ColorSpace::LINEAR_REC2020);
    ctx.fillStyle = Color(5.0f, 0.25f, 0.0f, 1.0f);
    ctx.fillRect(0, 0, 4, 4);
    auto d = ctx.getDefaultSurface().getImageData();
    const float * f = d->getFloatData() + (1 * 4 + 1) * 4;
    bool bgr = d->getChannelOrder() == ChannelOrder::BGR;
    CHECK((bgr ? f[2] : f[0]) == 5.0f && f[1] == 0.25f);
  }
#endif
  {  // The mapping on its own.
    Color c = linearRec2020ToSrgb(Color(1.0f, 1.0f, 1.0f, 0.5f));
    CHECK(std::fabs(c.red - 1.0f) < 1e-5f && std::fabs(c.alpha - 0.5f) < 1e-6f);
  }
  if (failures) std::printf("%d failures\n", failures);
  return failures ? 1 : 0;
}
