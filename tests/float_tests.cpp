// Float surfaces (PixelType::FLOAT32): colours above 1 and in the dark range
// survive, and blending, anti-aliasing and gradients are arithmetic on the
// numbers given. Run on whichever backend the build has and that has float
// surfaces (Cairo 1.17.2 or later, Quartz on macOS); GDI+ has none.
#if defined(CANVAS_HAS_CAIRO)
#include <canvas/ContextCairo.h>
namespace canvas { typedef ContextCairo TestContext; typedef CairoContextFactory TestFactory; }
#elif defined(CANVAS_HAS_QUARTZ)
#include <canvas/ContextQuartz2D.h>
namespace canvas {
  class TestContext : public ContextQuartz2D {
  public:
    TestContext(PixelSize size, unsigned int channels = 4, float scale = 1.0f, PixelType type = PixelType::UINT8)
      : ContextQuartz2D(std::make_shared<Quartz2DCache>(), size, channels, scale, type) { }
  };
  typedef Quartz2DContextFactory TestFactory;
}
#else
#include <canvas/Context.h>
#endif

#include <cmath>
#include <cstdio>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#if defined(CANVAS_HAS_CAIRO) || defined(CANVAS_HAS_QUARTZ)

struct F { float r, g, b, a; };  // premultiplied, as read back

static bool near_to(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static F fpx(Surface & surface, int x, int y) {
  auto d = surface.getImageData();
  CHECK(d->getPixelType() == PixelType::FLOAT32 && d->getNumChannels() == 4 && d->isPremultiplied());
  const float * p = d->getFloatData() + (size_t(y) * d->getWidth() + size_t(x)) * 4;
  if (d->getChannelOrder() == ChannelOrder::BGR) return { p[2], p[1], p[0], p[3] };
  return { p[0], p[1], p[2], p[3] };
}
// Printed with a failure, to see what a backend drew.
static void dump(const char * what, int x, F p) {
  std::printf("  %s x=%d: r=%g g=%g b=%g a=%g\n", what, x, p.r, p.g, p.b, p.a);
}

static F fpx(Context & ctx, int x, int y) { return fpx(ctx.getDefaultSurface(), x, y); }

static TestContext make(unsigned int w, unsigned int h, float scale = 1.0f) {
  return TestContext(PixelSize{ w, h }, 4, scale, PixelType::FLOAT32);
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  {
    TestFactory factory(1.0f, nullptr);
    CHECK(factory.supportsPixelType(PixelType::UINT8));
    if (!factory.supportsPixelType(PixelType::FLOAT32)) {
      std::printf("float surfaces are not supported by this backend: skipped\n");
      return 0;
    }
  }
  {  // A value above 1 and a small one survive, exactly, next to each other.
    auto ctx = make(8, 8);
    CHECK(ctx.getDefaultSurface().getPixelType() == PixelType::FLOAT32);
    ctx.fillStyle = Color(4.0f, 0.001f, 0.25f, 1.0f);
    ctx.fillRect(0, 0, 4, 8);
    ctx.fillStyle = Color(0.00002f, 1000.0f, 1.5f, 1.0f);
    ctx.fillRect(4, 0, 4, 8);
    F a = fpx(ctx, 1, 1), b = fpx(ctx, 6, 6);
    CHECK(a.r == 4.0f && a.g == 0.001f && a.b == 0.25f && a.a == 1.0f);
    CHECK(b.r == 0.00002f && b.g == 1000.0f && b.b == 1.5f && b.a == 1.0f);
    // Nothing was drawn outside.
    ctx.fillStyle = Color(0.0f, 0.0f, 0.0f, 0.0f);
  }
  {  // The read-back layout is stated: float, 4 channels, premultiplied.
    auto ctx = make(3, 2);
    auto d = ctx.getDefaultSurface().getImageData();
    CHECK(d->isFloat() && d->getNumChannels() == 4 && d->getWidth() == 3 && d->getHeight() == 2);
    CHECK(d->getBytesPerRow() == 3 * 4 * 4 && d->calculateSize() == 3 * 2 * 4 * 4);
    CHECK(d->isPremultiplied());
    auto img = ctx.getDefaultSurface().createImage(1.0f);
    CHECK(img->getData().isFloat());
  }
  {  // The headroom grows: earlier pixels keep their values when a larger one is drawn.
    auto ctx = make(8, 4);
    ctx.fillStyle = Color(0.003f, 2.0f, 1.0f, 1.0f);
    ctx.fillRect(0, 0, 2, 4);
    ctx.fillStyle = Color(100.0f, 0.0f, 0.0f, 1.0f);
    ctx.fillRect(2, 0, 2, 4);
    ctx.fillStyle = Color(5000.0f, 0.0f, 0.0f, 1.0f);
    ctx.fillRect(4, 0, 2, 4);
    F a = fpx(ctx, 0, 0), b = fpx(ctx, 3, 3), c = fpx(ctx, 5, 1), d = fpx(ctx, 7, 0);
    CHECK(a.r == 0.003f && a.g == 2.0f && a.b == 1.0f && a.a == 1.0f);
    CHECK(b.r == 100.0f && c.r == 5000.0f);
    CHECK(d.a == 0.0f && d.r == 0.0f);
  }
  {  // Alpha: a translucent colour is premultiplied, and blending over is
     // src * a + dst * (1 - a) on the given numbers.
    auto ctx = make(4, 4);
    ctx.fillStyle = Color(8.0f, 2.0f, 0.001f, 0.25f);
    ctx.fillRect(0, 0, 2, 4);
    F a = fpx(ctx, 0, 0);
    CHECK(near_to(a.r, 2.0, 1e-5) && near_to(a.g, 0.5, 1e-5) && near_to(a.b, 0.00025, 1e-7) && near_to(a.a, 0.25, 1e-6));
    ctx.fillStyle = Color(6.0f, 6.0f, 6.0f, 1.0f);
    ctx.fillRect(2, 0, 2, 4);
    ctx.fillStyle = Color(2.0f, 10.0f, 0.5f, 0.5f);
    ctx.fillRect(2, 0, 2, 4);
    F b = fpx(ctx, 3, 3);
    CHECK(near_to(b.r, 4.0, 1e-4) && near_to(b.g, 8.0, 1e-4) && near_to(b.b, 3.25, 1e-4) && b.a == 1.0f);
    // globalAlpha is alpha too.
    ctx.globalAlpha = 0.5f;
    ctx.fillStyle = Color(0.0f, 0.0f, 0.0f, 1.0f);
    ctx.fillRect(2, 0, 2, 4);
    ctx.globalAlpha = 1.0f;
    F c = fpx(ctx, 3, 3);
    CHECK(near_to(c.r, 2.0, 1e-4) && near_to(c.g, 4.0, 1e-4));
  }
  {  // Anti-aliasing: half coverage is half the value, with nothing clamped.
    auto ctx = make(8, 4);
    ctx.fillStyle = Color(6.0f, 3.0f, 0.5f, 1.0f);
    ctx.fillRect(0, 0, 4.5f, 4);
    F edge = fpx(ctx, 4, 1), inside = fpx(ctx, 1, 1), out = fpx(ctx, 6, 1);
    CHECK(inside.r == 6.0f && inside.g == 3.0f && inside.b == 0.5f);
    CHECK(near_to(edge.r, 3.0, 0.03) && near_to(edge.g, 1.5, 0.02) && near_to(edge.a, 0.5, 0.005));
    CHECK(out.a == 0.0f);
    // The same edge over a background is a blend of the two numbers.
    ctx.fillStyle = Color(2.0f, 2.0f, 2.0f, 1.0f);
    ctx.fillRect(4, 0, 4, 4);
    ctx.fillStyle = Color(10.0f, 10.0f, 10.0f, 1.0f);
    ctx.fillRect(0, 0, 4.5f, 4);
    F mix = fpx(ctx, 4, 2);
    CHECK(near_to(mix.r, 6.0, 0.04) && near_to(mix.a, 1.0, 1e-5));
    // A 1 px line on a .5 coordinate is one crisp row at its full value.
    auto line = make(8, 8);
    line.strokeStyle = Color(7.0f, 0.002f, 0.0f, 1.0f);
    line.lineWidth = 1.0f;
    line.beginPath().moveTo(0, 2.5f).lineTo(8, 2.5f).stroke();
    F row = fpx(line, 4, 2);
    CHECK(near_to(row.r, 7.0, 1e-5) && near_to(row.g, 0.002, 1e-7));
    CHECK(fpx(line, 4, 1).a == 0.0f && fpx(line, 4, 3).a == 0.0f);
    // Integer coordinates straddle two rows at half value each.
    auto line2 = make(8, 8);
    line2.strokeStyle = Color(8.0f, 8.0f, 8.0f, 1.0f);
    line2.beginPath().moveTo(0, 4).lineTo(8, 4).stroke();
    CHECK(near_to(fpx(line2, 4, 3).r, 4.0, 0.04) && near_to(fpx(line2, 4, 4).r, 4.0, 0.04));
  }
  {  // A linear gradient interpolates the given numbers: the value at a pixel
     // is the stop value at its centre, above 1 included.
    auto ctx = make(16, 2);
    auto g = ctx.createLinearGradient(0, 0, 16, 0);
    g.addColorStop(0.0f, Color(0.0f, 0.0f, 0.001f, 1.0f));
    g.addColorStop(1.0f, Color(4.0f, 8.0f, 0.001f, 1.0f));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 2);
    for (int x = 0; x < 16; x++) {
      double t = (x + 0.5) / 16;
      F p = fpx(ctx, x, 1);
      bool ok = near_to(p.r, 4.0 * t, 0.005) && near_to(p.g, 8.0 * t, 0.01) && near_to(p.b, 0.001, 5e-5);
      if (!ok && x % 4 == 0) dump("linear (want r=4t g=8t b=.001)", x, p);
      CHECK(ok);
    }
    CHECK(fpx(ctx, 15, 0).r > 3.5f);
  }
  {  // Three stops, with alpha: interpolated straight, not premultiplied.
    auto ctx = make(16, 1);
    auto g = ctx.createLinearGradient(0, 0, 16, 0);
    g.addColorStop(0.0f, Color(0.0f, 0.0f, 0.0f, 1.0f));
    g.addColorStop(0.5f, Color(16.0f, 16.0f, 16.0f, 1.0f));
    g.addColorStop(1.0f, Color(16.0f, 16.0f, 16.0f, 0.0f));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 1);
    F mid = fpx(ctx, 8, 0), q = fpx(ctx, 4, 0), last = fpx(ctx, 15, 0);
    dump("three stops (want r=9 a=1)", 4, q);
    dump("three stops (want r=15 a=.94)", 8, mid);
    dump("three stops (want a=.03)", 15, last);
    CHECK(near_to(mid.a, 0.9375, 0.01) && near_to(mid.r, 16.0 * mid.a, 0.05));
    CHECK(near_to(q.r, 16.0 * (4.5 / 8), 0.02) && q.a == 1.0f);
    // Past the middle the colour is 16 and alpha falls: premultiplied r = 16 * a.
    CHECK(near_to(last.a, 1.0 - (15.5 / 16 - 0.5) / 0.5, 0.01));
    CHECK(near_to(last.r, 16.0 * last.a, 0.05));
  }
  {  // A radial gradient too.
    auto ctx = make(16, 16);
    auto g = ctx.createRadialGradient(8, 8, 0, 8, 8, 8);
    g.addColorStop(0.0f, Color(8.0f, 8.0f, 8.0f, 1.0f));
    g.addColorStop(1.0f, Color(0.0f, 0.0f, 0.0f, 1.0f));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 16);
    F c = fpx(ctx, 8, 8), side = fpx(ctx, 12, 8);
    dump("radial centre (want r=7.5)", 8, c);
    dump("radial side (want r=3.5)", 12, side);
    double dc = std::hypot(0.5, 0.5), ds = std::hypot(4.5, 0.5);
    CHECK(near_to(c.r, 8.0 * (1 - dc / 8), 0.02) && c.r > 1.0f);
    CHECK(near_to(side.r, 8.0 * (1 - ds / 8), 0.02));
  }
  {  // Gradient with a global alpha, and a stroke gradient where supported.
    auto ctx = make(16, 4);
    auto g = ctx.createLinearGradient(0, 0, 16, 0);
    g.addColorStop(0.0f, Color(4.0f, 4.0f, 4.0f, 1.0f));
    g.addColorStop(1.0f, Color(4.0f, 4.0f, 4.0f, 1.0f));
    ctx.globalAlpha = 0.5f;
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 2);
    F f = fpx(ctx, 5, 0);
    CHECK(near_to(f.r, 2.0, 0.01) && near_to(f.a, 0.5, 0.005));
#ifdef CANVAS_HAS_CAIRO
    // Quartz does not stroke with gradients (8-bit either).
    ctx.strokeStyle = g;
    ctx.lineWidth = 2.0f;
    ctx.beginPath().moveTo(0, 3).lineTo(16, 3).stroke();
    F s = fpx(ctx, 5, 3);
    CHECK(near_to(s.r, 2.0, 0.02) && near_to(s.a, 0.5, 0.005));
#endif
  }
  {  // COPY (clearRect) replaces, and the clip limits a fill.
    auto ctx = make(8, 8);
    ctx.fillStyle = Color(3.0f, 3.0f, 3.0f, 1.0f);
    ctx.fillRect(0, 0, 8, 8);
    ctx.clearRect(0, 0, 4, 8);
    CHECK(fpx(ctx, 1, 1).a == 0.0f && fpx(ctx, 1, 1).r == 0.0f && fpx(ctx, 6, 1).r == 3.0f);
    ctx.save();
    ctx.beginPath().rect(0, 0, 2, 8).clip();
    ctx.fillStyle = Color(9.0f, 9.0f, 9.0f, 1.0f);
    ctx.fillRect(0, 0, 8, 8);
    ctx.restore();
    CHECK(fpx(ctx, 1, 1).r == 9.0f && fpx(ctx, 3, 1).r == 0.0f && fpx(ctx, 6, 1).r == 3.0f);
    // A circular clip is anti-aliased: partial values at the edge, full inside.
    auto c2 = make(16, 16);
    c2.save();
    c2.beginPath().arc(8, 8, 5.3f, 0, 6.2831853f).clip();
    c2.fillStyle = Color(5.0f, 5.0f, 5.0f, 1.0f);
    c2.fillRect(0, 0, 16, 16);
    c2.restore();
    CHECK(fpx(c2, 8, 8).r == 5.0f);
    bool partial = false;
    for (int x = 0; x < 16; x++) {
      F p = fpx(c2, x, 8);
      if (p.a > 0.05f && p.a < 0.95f) { partial = true; CHECK(near_to(p.r, 5.0 * p.a, 0.02)); }
    }
    CHECK(partial);
  }
  {  // Text with a colour above 1 reaches above 1.
    auto ctx = make(48, 24);
    ctx.fillStyle = Color(8.0f, 4.0f, 0.5f, 1.0f);
    ctx.font.size = 16;
    ctx.fillText("HHHH", 2, 18);
    float max_r = 0.0f, max_g = 0.0f;
    auto d = ctx.getDefaultSurface().getImageData();
    for (size_t i = 0; i < 48 * 24; i++) {
      max_r = std::max(max_r, d->getFloatData()[4 * i]);
      max_g = std::max(max_g, d->getFloatData()[4 * i + 1]);
    }
    CHECK(max_r > 7.5f && max_r <= 8.001f);
    CHECK(max_g > 3.7f && max_g <= 4.001f);
    ctx.strokeStyle = Color(6.0f, 6.0f, 6.0f, 1.0f);
    ctx.lineWidth = 1.0f;
    ctx.strokeText("I", 30, 18);
    d = ctx.getDefaultSurface().getImageData();
    float max_stroke = 0.0f;
    for (int y = 0; y < 24; y++) for (int x = 28; x < 48; x++) max_stroke = std::max(max_stroke, d->getFloatData()[4 * (y * 48 + x)]);
    CHECK(max_stroke > 4.0f);
    // Subpixel text (component alpha) too.
    auto lcd = make(48, 24);
    lcd.font.size = 16;
    lcd.font.cleartype = true;
    lcd.fillStyle = Color(8.0f, 4.0f, 0.5f, 1.0f);
    lcd.fillText("HHHH", 2, 18);
    d = lcd.getDefaultSurface().getImageData();
    max_r = 0.0f;
    for (size_t i = 0; i < 48 * 24; i++) max_r = std::max(max_r, d->getFloatData()[4 * i]);
    CHECK(max_r > 7.5f && max_r <= 8.001f);
  }
  {  // Images: a float surface drawn onto another keeps its values, including
     // when the two surfaces have different headrooms.
    auto src = make(4, 4);
    src.fillStyle = Color(30.0f, 0.004f, 1.0f, 1.0f);
    src.fillRect(0, 0, 4, 4);
    auto dst = make(8, 4);
    dst.fillStyle = Color(0.5f, 0.5f, 0.5f, 1.0f);
    dst.fillRect(0, 0, 8, 4);
    dst.drawImage(src, 4, 0, 4, 4);
    F a = fpx(dst, 5, 1), b = fpx(dst, 1, 1);
    CHECK(near_to(a.r, 30.0, 1e-3) && near_to(a.g, 0.004, 1e-6) && near_to(a.b, 1.0, 1e-5));
    CHECK(near_to(b.r, 0.5, 1e-6));
    // The other way round: the drawn surface has the larger headroom.
    auto big = make(4, 4);
    big.fillStyle = Color(200.0f, 200.0f, 200.0f, 1.0f);
    big.fillRect(0, 0, 4, 4);
    auto small = make(8, 4);
    small.fillStyle = Color(1.5f, 0.25f, 0.0f, 1.0f);
    small.fillRect(0, 0, 8, 4);
    small.drawImage(big, 4, 0, 4, 4);
    CHECK(near_to(fpx(small, 5, 1).r, 200.0, 0.01) && near_to(fpx(small, 1, 1).r, 1.5, 1e-6) && near_to(fpx(small, 1, 1).g, 0.25, 1e-6));
    // Scaling up keeps the range.
    auto up = make(8, 8);
    up.drawImage(src, 0, 0, 8, 8);
    CHECK(near_to(fpx(up, 4, 4).r, 30.0, 0.01));
    // Float ImageData (straight, RGB) in, with a value above 1.
    float px[4 * 4];
    for (int i = 0; i < 4; i++) { px[4 * i] = 12.0f; px[4 * i + 1] = 0.5f; px[4 * i + 2] = 0.001f; px[4 * i + 3] = 0.5f; }
    ImageData img(px, 2, 2, 4, PixelType::FLOAT32);
    CHECK(img.isFloat() && !img.isPremultiplied() && img.getBytesPerRow() == 32);
    auto ctx = make(2, 2);
    ctx.drawImage(img, 0, 0, 2, 2);
    F p = fpx(ctx, 1, 1);
    CHECK(near_to(p.r, 6.0, 1e-3) && near_to(p.g, 0.25, 1e-4) && near_to(p.b, 0.0005, 1e-6) && near_to(p.a, 0.5, 1e-5));
    // A surface made from float ImageData reads back the same numbers.
    auto from_data = ctx.createSurface(img);
    CHECK(from_data->getPixelType() == PixelType::FLOAT32);
    F q = fpx(*from_data, 0, 0);
    CHECK(near_to(q.r, 6.0, 1e-5) && near_to(q.a, 0.5, 1e-6));
    CHECK(near_to(fpx(*from_data, 1, 1).g, 0.25, 1e-6));
    // Bytes onto a float surface are v / 255 as they are.
    unsigned char bytes[2 * 4] = { 255, 51, 0, 255, 255, 51, 0, 255 };
    ImageData b8(bytes, 2, 1, 4);
    auto c2 = make(2, 2);
    c2.fillStyle = Color(4.0f, 4.0f, 4.0f, 1.0f);
    c2.fillRect(0, 0, 2, 2);
    c2.drawImage(b8, 0, 0, 2, 1);
    F e = fpx(c2, 1, 0);
    CHECK(fpx(c2, 1, 1).r == 4.0f);
    CHECK(near_to(e.r, 1.0, 1e-5) && near_to(e.g, 0.2, 1e-5) && e.b == 0.0f);
  }
  {  // Bytes surfaces still clamp (and don't wrap) a colour above 1, and a
     // float image drawn on one is clamped.
    TestContext ctx(PixelSize{ 4, 4 });
    ctx.fillStyle = Color(4.0f, 0.5f, -1.0f, 1.0f);
    ctx.fillRect(0, 0, 4, 4);
    auto d = ctx.getDefaultSurface().getImageData();
    CHECK(d->getPixelType() == PixelType::UINT8);
    const unsigned char * p = d->getData();
    CHECK(p[2] == 255 && p[3] == 255);
    float px[4] = { 9.0f, 0.0f, 0.0f, 1.0f };
    ImageData img(px, 1, 1, 4, PixelType::FLOAT32);
    TestContext c2(PixelSize{ 1, 1 });
    c2.drawImage(img, 0, 0, 1, 1);
    CHECK(c2.getDefaultSurface().getImageData()->getData()[2] == 255);
  }
  {  // Shadows with a colour above 1 (the mask is coverage; the colour is float).
    auto ctx = make(24, 24);
    ctx.shadowColor = Color(6.0f, 6.0f, 6.0f, 1.0f);
    ctx.shadowOffsetX = 8;
    ctx.fillStyle = Color(0.0f, 0.0f, 1.0f, 1.0f);
    ctx.fillRect(2, 8, 6, 6);
    F shadow = fpx(ctx, 13, 10), shape = fpx(ctx, 4, 10);
    CHECK(near_to(shadow.r, 6.0, 0.05) && shadow.a > 0.99f);
    CHECK(shape.b == 1.0f);
  }
  {  // Display scale: logical sizes and coordinates, float pixels.
    auto ctx = make(16, 16, 2.0f);
    ctx.fillStyle = Color(3.0f, 3.0f, 3.0f, 1.0f);
    ctx.fillRect(1, 1, 2, 2);
    CHECK(fpx(ctx, 2, 2).r == 3.0f && fpx(ctx, 5, 5).r == 3.0f && fpx(ctx, 6, 6).a == 0.0f);
  }
  {  // resize() keeps the float type and clears.
    auto ctx = make(4, 4);
    ctx.fillStyle = Color(3.0f, 3.0f, 3.0f, 1.0f);
    ctx.fillRect(0, 0, 4, 4);
    ctx.resize(PixelSize{ 6, 6 });
    CHECK(ctx.getActualWidth() == 6 && ctx.getDefaultSurface().getPixelType() == PixelType::FLOAT32);
    CHECK(fpx(ctx, 1, 1).a == 0.0f);
    ctx.fillStyle = Color(0.5f, 0.5f, 0.5f, 1.0f);
    ctx.fillRect(0, 0, 6, 6);
    CHECK(fpx(ctx, 1, 1).r == 0.5f);
  }
  {  // Through the factory and context; the channel count is checked.
    TestFactory factory(1.0f, nullptr);
    auto ctx = factory.createContext(PixelSize{ 4, 4 }, 4, PixelType::FLOAT32);
    CHECK(ctx->getDefaultSurface().getPixelType() == PixelType::FLOAT32);
    auto surface = factory.createSurface(PixelSize{ 4, 4 }, 4, PixelType::FLOAT32);
    CHECK(surface->getPixelType() == PixelType::FLOAT32);
    bool threw = false;
    try { factory.createSurface(PixelSize{ 4, 4 }, 3, PixelType::FLOAT32); } catch (const UnsupportedFormatException &) { threw = true; }
    CHECK(threw);
    threw = false;
    try { factory.createSurface(PixelSize{ 4, 4 }, 1, PixelType::FLOAT32); } catch (const UnsupportedFormatException &) { threw = true; }
    CHECK(threw);
  }
  if (failures) std::printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

#else

int main() {
  std::printf("this backend has no float surfaces: skipped\n");
  return 0;
}

#endif
