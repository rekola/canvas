// ColorSpace::LINEAR_REC2020: an 8-bit surface is given what it can show of a
// linear Rec.2020 colour (gamut mapped, light past white compressed), a float
// surface the numbers as given, and the default changes nothing.
#if defined(CANVAS_HAS_CAIRO)
#include <canvas/ContextCairo.h>
namespace canvas { typedef ContextCairo TestContext; }
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

#include <cmath>
#include <cstdio>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

struct P { int r, g, b; };

static P bytes(Context & ctx, int x, int y) {
  auto d = ctx.getDefaultSurface().getImageData();
  const unsigned char * p = d->getData() + (size_t(y) * d->getWidth() + size_t(x)) * d->getNumChannels();
  if (d->getChannelOrder() == ChannelOrder::BGR) return { p[2], p[1], p[0] };
  return { p[0], p[1], p[2] };
}

static P draw8(ColorSpace cs, const Color & c) {
  TestContext ctx(PixelSize{ 4, 4 }, 4, 1.0f, PixelType::UINT8);
  ctx.setInputColorSpace(cs);
  ctx.fillStyle = c;
  ctx.fillRect(0, 0, 4, 4);
  return bytes(ctx, 1, 1);
}

int main() {
  {  // The default is as given: 8-bit output is unchanged.
    P p = draw8(ColorSpace::AS_GIVEN, Color(1.0f, 0.0f, 0.5f, 1.0f));
    CHECK(p.r == 255 && p.g == 0 && p.b == 128);
  }
  {  // White stays white; a mid grey is encoded (linear 0.214 is half in sRGB).
    P w = draw8(ColorSpace::LINEAR_REC2020, Color(1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(w.r == 255 && w.g == 255 && w.b == 255);
    P g = draw8(ColorSpace::LINEAR_REC2020, Color(0.2140f, 0.2140f, 0.2140f, 1.0f));
    CHECK(std::abs(g.r - 128) <= 1 && std::abs(g.g - 128) <= 1 && std::abs(g.b - 128) <= 1);
    P k = draw8(ColorSpace::LINEAR_REC2020, Color(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(k.r == 0 && k.g == 0 && k.b == 0);
  }
  {  // Light past white is compressed and washes toward white: brighter is paler.
    P a = draw8(ColorSpace::LINEAR_REC2020, Color(1.2f, 0.1f, 0.1f, 1.0f));
    P b = draw8(ColorSpace::LINEAR_REC2020, Color(3.0f, 0.1f, 0.1f, 1.0f));
    P c = draw8(ColorSpace::LINEAR_REC2020, Color(50.0f, 0.1f, 0.1f, 1.0f));
    CHECK(a.r <= 255 && b.r == 255 && c.r == 255);
    CHECK(b.g > a.g && c.g > b.g && c.b > b.b);
  }
  {  // A colour outside sRGB is mapped in, not clipped to a corner.
    P p = draw8(ColorSpace::LINEAR_REC2020, Color(0.0f, 1.0f, 0.0f, 1.0f));
    // Pure Rec.2020 green has a negative red in sRGB; its chroma is reduced onto
    // the gamut edge (red 0) and its hue kept, where a clip would give 0, 255, 0.
    CHECK(p.r == 0 && p.g > 200 && p.g < 255 && p.b > 50);
  }
  {  // A gradient across white follows the map, not a line between its mapped ends.
    TestContext ctx(PixelSize{ 64, 4 }, 4, 1.0f, PixelType::UINT8);
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
  {  // A float surface keeps the numbers.
    TestContext ctx(PixelSize{ 4, 4 }, 4, 1.0f, PixelType::FLOAT32);
    ctx.setInputColorSpace(ColorSpace::LINEAR_REC2020);
    ctx.fillStyle = Color(5.0f, 0.25f, 0.0f, 1.0f);
    ctx.fillRect(0, 0, 4, 4);
    auto d = ctx.getDefaultSurface().getImageData();
    const float * f = d->getFloatData() + (1 * 4 + 1) * 4;
    bool bgr = d->getChannelOrder() == ChannelOrder::BGR;
    CHECK((bgr ? f[2] : f[0]) == 5.0f && f[1] == 0.25f);
  }
  {  // The mapping on its own.
    Color c = linearRec2020ToSrgb(Color(1.0f, 1.0f, 1.0f, 0.5f));
    CHECK(std::fabs(c.red - 1.0f) < 1e-5f && std::fabs(c.alpha - 0.5f) < 1e-6f);
  }
  if (failures) std::printf("%d failures\n", failures);
  return failures ? 1 : 0;
}
