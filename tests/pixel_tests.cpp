// Pixel tests for what a client draws, run on whichever backend the build
// has (Cairo, GDI+ or Quartz). The surface's memory is read back through
// Surface::createImage().
#include <canvas/PerlinSurface.h>

#if defined(CANVAS_HAS_CAIRO)
#include <canvas/ContextCairo.h>
namespace canvas { typedef ContextCairo TestContext; typedef CairoContextFactory TestFactory; }
#elif defined(CANVAS_HAS_GDIPLUS)
#include <canvas/ContextGDIPlus.h>
namespace canvas { typedef ContextGDIPlus TestContext; typedef GDIPlusContextFactory TestFactory; }
#elif defined(CANVAS_HAS_QUARTZ)
#include <canvas/ContextQuartz2D.h>
namespace canvas {
  // The Quartz context takes a shared cache; the tests don't care.
  class TestContext : public ContextQuartz2D {
  public:
    TestContext(unsigned int w, unsigned int h, unsigned int channels = 4, float scale = 1.0f)
      : ContextQuartz2D(std::make_shared<Quartz2DCache>(), w, h, channels, scale) { }
    TestContext(PixelSize size, unsigned int channels = 4, float scale = 1.0f)
      : ContextQuartz2D(std::make_shared<Quartz2DCache>(), size, channels, scale) { }
  };
  typedef Quartz2DContextFactory TestFactory;
}
#else
#error "pixel tests need the Cairo, GDI+ or Quartz backend"
#endif

#include <cstdio>
#include <exception>
#include <algorithm>
#include <cstdlib>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

struct Px { int b, g, r, a; };

static Px pixel(Context & ctx, unsigned x, unsigned y) {
  auto img = ctx.getDefaultSurface().createImage(1.0f);
  const ImageData & d = img->getData();
  CHECK(d.getNumChannels() == 4);
  const unsigned char * p = d.getData() + y * d.getBytesPerRow() + x * 4;
  return { p[0], p[1], p[2], p[3] };
}

static bool close_to(int a, int b, int tol = 2) { return std::abs(a - b) <= tol; }
static bool near(double a, double b) { return std::abs(a - b) < 1e-3; }

// The alpha of a column, summed over all rows.
static int columnCoverage(Context & ctx, unsigned x) {
  int sum = 0;
  for (unsigned y = 0; y < ctx.getActualHeight(); y++) sum += pixel(ctx, x, y).a;
  return sum;
}

// Opens a file whose name is UTF-8 (the Windows fopen() would read it as ANSI).
static FILE * openUtf8(const char * path) {
#ifdef _WIN32
  wchar_t wide[256];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 256)) return nullptr;
  return _wfopen(wide, L"wb");
#else
  return std::fopen(path, "wb");
#endif
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);  // keep failures visible if a test crashes
#if defined(CANVAS_HAS_GDIPLUS) && !defined(CANVAS_HAS_CAIRO)
  ContextGDIPlus::initialize();
#endif
  {  // A filled rectangle: exact colour inside, untouched outside.
    TestContext ctx(8, 8);
    ctx.fillStyle = "#ff0000";
    ctx.fillRect(2, 2, 4, 4);
    Px in = pixel(ctx, 3, 3), out = pixel(ctx, 0, 0);
    CHECK(in.r == 255 && in.g == 0 && in.b == 0 && in.a == 255);
    CHECK(out.a == 0 && out.r == 0);
    // Integer coordinates are pixel edges, as in HTML5 canvas.
    CHECK(pixel(ctx, 2, 2).a == 255 && pixel(ctx, 5, 5).a == 255);
    CHECK(pixel(ctx, 1, 2).a == 0 && pixel(ctx, 6, 5).a == 0 && pixel(ctx, 2, 1).a == 0 && pixel(ctx, 5, 6).a == 0);
  }
  {  // Premultiplied alpha: 50% white is stored as 128 in every channel.
    TestContext ctx(4, 4);
    ctx.fillStyle = Color(1.0f, 1.0f, 1.0f, 0.5f);
    ctx.fillRect(0, 0, 4, 4);
    Px p = pixel(ctx, 1, 1);
    CHECK(close_to(p.a, 128) && close_to(p.r, 128) && close_to(p.g, 128) && close_to(p.b, 128));
  }
  {  // An antialiased diagonal line has partial coverage next to its centre
     // and nothing far from it.
    TestContext ctx(16, 16);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 1.0f;
    ctx.beginPath().moveTo(0, 0).lineTo(16, 16).stroke();
    bool partial = false;
    for (unsigned x = 1; x < 15; x++) {
      Px side = pixel(ctx, x + 1, x);
      if (side.a > 0 && side.a < 255) partial = true;
    }
    CHECK(partial);
    CHECK(pixel(ctx, 15, 0).a == 0);
  }
  {  // A 1 px line on an integer coordinate straddles two rows at half
     // coverage; on a .5 coordinate it is one crisp row (HTML5 canvas).
    TestContext ctx(8, 8);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 1.0f;
    ctx.beginPath().moveTo(0, 2).lineTo(8, 2).stroke();
    ctx.beginPath().moveTo(0, 5.5).lineTo(8, 5.5).stroke();
    CHECK(close_to(pixel(ctx, 4, 1).a, 128) && close_to(pixel(ctx, 4, 2).a, 128));
    CHECK(pixel(ctx, 4, 5).a == 255 && pixel(ctx, 4, 4).a == 0 && pixel(ctx, 4, 6).a == 0);
  }
  {  // A polyline covers each of its segments.
    TestContext ctx(16, 16);
    ctx.strokeStyle = "#00ff00";
    ctx.lineWidth = 2.0f;
    ctx.beginPath().moveTo(1, 8).lineTo(8, 8).lineTo(8, 15).stroke();
    CHECK(pixel(ctx, 4, 8).g > 200);
    CHECK(pixel(ctx, 8, 12).g > 200);
    CHECK(pixel(ctx, 12, 3).a == 0);
  }
  {  // A linear gradient runs monotonically between its stops.
    TestContext ctx(16, 2);
    auto g = ctx.createLinearGradient(0, 0, 16, 0);
    g.addColorStop(0.0f, "#000000");
    g.addColorStop(1.0f, "#ffffff");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 2);
    Px a = pixel(ctx, 1, 0), b = pixel(ctx, 8, 0), c = pixel(ctx, 14, 0);
    CHECK(a.r < b.r && b.r < c.r);
    CHECK(a.r < 40 && c.r > 215);
  }
  {  // An image drawn onto a context reads back where it was placed. The
     // ImageData is straight RGBA (the default), so (255, 0, 0, 255) is red.
    TestContext ctx(8, 8);
    unsigned char rgba[4 * 2 * 2];
    for (int i = 0; i < 4; i++) { rgba[4 * i] = 255; rgba[4 * i + 1] = 0; rgba[4 * i + 2] = 0; rgba[4 * i + 3] = 255; }
    ImageData img(rgba, 2, 2, 4);
    ctx.drawImage(img, 4, 4, 2, 2);
    for (unsigned y = 4; y < 6; y++)
      for (unsigned x = 4; x < 6; x++) {
        Px in = pixel(ctx, x, y);
        CHECK(in.a == 255 && in.r == 255 && in.g == 0 && in.b == 0);
      }
    CHECK(pixel(ctx, 3, 4).a == 0 && pixel(ctx, 6, 5).a == 0);
    CHECK(pixel(ctx, 1, 1).a == 0);
  }
  {  // Straight alpha is premultiplied on the way in: 50% blue.
    TestContext ctx(2, 2);
    unsigned char rgba[4] = { 0, 0, 255, 128 };
    ctx.drawImage(ImageData(rgba, 1, 1, 4), 0, 0, 1, 1);
    Px p = pixel(ctx, 0, 0);
    CHECK(close_to(p.a, 128) && close_to(p.b, 128) && p.r == 0 && p.g == 0);
  }
  {  // The same pixel as premultiplied BGRA (what createImage() hands out) draws the same.
    TestContext ctx(2, 2);
    unsigned char bgra[4] = { 128, 0, 0, 128 };
    ImageData img(bgra, 1, 1, 4);
    img.setFormat(ChannelOrder::BGR, true);
    ctx.drawImage(img, 0, 0, 1, 1);
    Px p = pixel(ctx, 0, 0);
    CHECK(close_to(p.a, 128) && close_to(p.b, 128) && p.r == 0 && p.g == 0);
  }
  {  // 3 channels follow the channel order too.
    TestContext ctx(2, 1);
    unsigned char rgb[3] = { 255, 0, 0 };
    ctx.drawImage(ImageData(rgb, 1, 1, 3), 0, 0, 1, 1);
    ImageData bgr(rgb, 1, 1, 3);
    bgr.setFormat(ChannelOrder::BGR, false);
    ctx.drawImage(bgr, 1, 0, 1, 1);
    CHECK(pixel(ctx, 0, 0).r == 255 && pixel(ctx, 0, 0).b == 0);
    CHECK(pixel(ctx, 1, 0).b == 255 && pixel(ctx, 1, 0).r == 0);
  }
  {  // One channel is an alpha mask. Its rows are 5 bytes wide and Cairo pads
     // them to 8; every pixel must survive the round trip.
    const unsigned w = 5, h = 3;
    unsigned char mask[w * h];
    for (unsigned i = 0; i < w * h; i++) mask[i] = (unsigned char)(10 + i * 10);
    TestContext ctx(w, h, 1);
    ctx.drawImage(ImageData(mask, w, h, 1), 0, 0, w, h);
    auto img = ctx.getDefaultSurface().createImage(1.0f);
    const ImageData & d = img->getData();
    CHECK(d.getNumChannels() == 1 && d.getWidth() == w && d.getHeight() == h);
    bool same = true;
    for (unsigned i = 0; i < w * h; i++) if (std::abs(d.getData()[i] - mask[i]) > 1) same = false;
    CHECK(same);
  }
  {  // Pixels read back from a surface say what they are.
    TestContext ctx(2, 2);
    ctx.fillStyle = "#ff0000";
    ctx.fillRect(0, 0, 2, 2);
    auto img = ctx.getDefaultSurface().createImage(1.0f);
    CHECK(img->getData().getChannelOrder() == ChannelOrder::BGR && img->getData().isPremultiplied());
  }
  {  // A shadow of an odd width is not sheared: it is symmetric about the shape.
    TestContext ctx(15, 15);
    ctx.shadowColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    ctx.shadowBlur = 2.0f;
    ctx.shadowOffsetX = 0.0f;
    ctx.fillStyle = "#ffffff";
    ctx.fillRect(6, 6, 3, 3);
    Px l = pixel(ctx, 5, 7), r = pixel(ctx, 9, 7);
    CHECK(close_to(l.a, r.a, 2) && l.a > 20);
  }
  {  // loadImage() asks the factory's converter for the path, and the name
     // may be UTF-8.
    const char * path = "canvas_test_\xc3\xa4.ppm";
    FILE * f = openUtf8(path);
    CHECK(f != nullptr);
    if (f) {
      std::fputs("P6\n1 1\n255\n", f);
      std::fputc(0, f); std::fputc(255, f); std::fputc(0, f);
      std::fclose(f);
      struct Converter : FilenameConverter {
	bool convert(const std::string & in, std::string & out) override { out = in == "icon" ? "canvas_test_\xc3\xa4.ppm" : in; return true; }
      } converter;
      TestFactory factory(1.0f, &converter);
      try {
	auto image = factory.loadImage("icon");
	const ImageData & d = image->getData();
	CHECK(d.getWidth() == 1 && d.getNumChannels() == 3 && d.getData()[1] == 255);
	TestContext ctx(1, 1);
	ctx.drawImage(d, 0, 0, 1, 1);
	CHECK(pixel(ctx, 0, 0).g == 255 && pixel(ctx, 0, 0).r == 0);
      } catch (const std::exception & e) {
	std::printf("loading failed: %s\n", e.what());
	failures++;
      }
#ifdef _WIN32
      wchar_t wide[256];
      MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 256);
      _wremove(wide);
#else
      std::remove(path);
#endif
    }
  }
  {  // Each gradient is its own: a second one doesn't change the first.
    TestContext ctx(16, 2);
    auto g1 = ctx.createLinearGradient(0, 0, 16, 0);
    g1.addColorStop(0.0f, "#ff0000");
    g1.addColorStop(1.0f, "#ff0000");
    auto g2 = ctx.createLinearGradient(0, 0, 16, 0);
    g2.addColorStop(0.0f, "#0000ff");
    g2.addColorStop(1.0f, "#0000ff");
    CHECK(g1.getColors().size() == 2 && g2.getColors().size() == 2);
    ctx.fillStyle = g1;
    ctx.fillRect(0, 0, 16, 2);
    Px p = pixel(ctx, 8, 1);
    CHECK(p.r == 255 && p.b == 0);
  }
  {  // The current path survives restore(); the drawing state doesn't.
    TestContext ctx(8, 8);
    ctx.beginPath();
    ctx.save();
    ctx.fillStyle = "#00ff00";
    ctx.rect(0, 0, 8, 8);
    ctx.restore();
    ctx.fill();
    Px p = pixel(ctx, 4, 4);
    CHECK(p.a == 255 && p.g == 0);
  }
  {  // Setters and transforms chain into path calls.
    TestContext ctx(8, 8);
    ctx.fillStyle("#0000ff").translate(2, 2).beginPath().rect(0, 0, 2, 2).fill();
    CHECK(pixel(ctx, 3, 3).b == 255 && pixel(ctx, 1, 1).a == 0);
    ctx.lineWidth(2.0f).strokeStyle(Color(1.0f, 0.0f, 0.0f, 1.0f)).resetTransform().beginPath().moveTo(0, 7).lineTo(8, 7).stroke();
    CHECK(pixel(ctx, 4, 7).r == 255);
  }
  {  // Assigning a style from another context keeps the owner.
    TestContext a(4, 4), b(4, 4);
    b.fillStyle = "#ff0000";
    a.fillStyle = b.fillStyle;
    a.fillStyle("#00ff00").fillRect(0, 0, 4, 4);
    CHECK(pixel(a, 1, 1).g == 255 && pixel(b, 1, 1).a == 0);
  }
  {  // Points are transformed when added: a path built across translate().
    TestContext ctx(16, 16);
    ctx.fillStyle = "#ff0000";
    ctx.beginPath().rect(0, 0, 2, 2).translate(8, 8).rect(0, 0, 2, 2).fill();
    CHECK(pixel(ctx, 1, 1).r == 255 && pixel(ctx, 9, 9).r == 255);
    CHECK(pixel(ctx, 5, 5).a == 0 && pixel(ctx, 1, 9).a == 0);
  }
  {  // Line width is the matrix's scale, not its `a`: rotated by 90 degrees it still draws.
    TestContext ctx(8, 8);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 2.0f;
    ctx.rotate(M_PI / 2);
    ctx.beginPath().moveTo(0, -4).lineTo(8, -4).stroke();  // x = 4 on the canvas, 2 px wide
    CHECK(pixel(ctx, 3, 4).a > 200 && pixel(ctx, 4, 4).a > 200 && pixel(ctx, 1, 4).a == 0);
  }
  {  // Arcs follow rotation: a half circle under rotate(PI) lands below the centre.
    TestContext ctx(16, 16);
    ctx.fillStyle = "#ffffff";
    ctx.translate(8, 8).rotate(M_PI);
    ctx.beginPath().arc(0, 0, 6, M_PI, 2 * M_PI, false).fill();  // upper half in user space
    CHECK(pixel(ctx, 8, 11).a == 255 && pixel(ctx, 8, 5).a == 0);
  }
  {  // clip() uses the transform in force and intersects with the previous clip.
    TestContext ctx(16, 16);
    ctx.fillStyle = "#ffffff";
    ctx.save();
    ctx.translate(4, 4).beginPath().rect(0, 0, 8, 8).clip();
    ctx.beginPath().rect(4, 4, 12, 12).clip();  // intersects: device (8..12, 8..12)
    ctx.resetTransform().fillRect(0, 0, 16, 16);
    CHECK(pixel(ctx, 10, 10).a == 255 && pixel(ctx, 6, 6).a == 0 && pixel(ctx, 13, 13).a == 0 && pixel(ctx, 2, 2).a == 0);
    ctx.restore();
    ctx.fillRect(0, 0, 16, 16);
    CHECK(pixel(ctx, 2, 2).a == 255);
  }
  {  // clip() leaves the current path.
    TestContext ctx(8, 8);
    ctx.fillStyle = "#ffffff";
    ctx.beginPath().rect(0, 0, 4, 8).clip().fill();
    CHECK(pixel(ctx, 1, 1).a == 255 && pixel(ctx, 6, 1).a == 0);
  }
  {  // isPointInPath() takes device coordinates, with the path in user space.
    TestContext ctx(16, 16);
    Path2D path;
    path.rect(0, 0, 4, 4);
    ctx.translate(8, 8);
    CHECK(ctx.isPointInPath(path, 9, 9) && !ctx.isPointInPath(path, 2, 2));
  }
  {  // A filter style draws its solid colour, not whatever was drawn before it.
    TestContext ctx(4, 2);
    ctx.fillStyle = "#ff0000";
    ctx.fillRect(0, 0, 2, 2);
    canvas::Style filter(&ctx);
    filter.setType(canvas::Style::FILTER);
    ctx.fillStyle = filter;
    ctx.fillRect(2, 0, 2, 2);
    Px p = pixel(ctx, 3, 1);
    CHECK(p.a == 255 && p.r == 0 && p.g == 0 && p.b == 0);
  }
  {  // Line caps: a square cap extends the line by half its width, a round one
     // rounds the corner, butt stops at the end point.
    Px corner[3], beyond[3];
    const char * caps[3] = { "butt", "round", "square" };
    for (int i = 0; i < 3; i++) {
      TestContext ctx(16, 16);
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 4.0f;
      ctx.lineCap = caps[i];
      ctx.beginPath().moveTo(4, 8).lineTo(12, 8).stroke();
      corner[i] = pixel(ctx, 2, 6);
      beyond[i] = pixel(ctx, 3, 7);
    }
    CHECK(beyond[0].a == 0 && corner[0].a == 0);
    CHECK(beyond[1].a > 200 && corner[1].a < 128);
    CHECK(beyond[2].a == 255 && corner[2].a == 255);
  }
  {  // Line joins: the outer corner of a right angle is filled by a miter, cut by a bevel.
    Px outer[2];
    const char * joins[2] = { "miter", "bevel" };
    for (int i = 0; i < 2; i++) {
      TestContext ctx(16, 16);
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 4.0f;
      ctx.lineJoin = joins[i];
      ctx.beginPath().moveTo(4, 14).lineTo(4, 4).lineTo(14, 4).stroke();
      outer[i] = pixel(ctx, 2, 2);
    }
    CHECK(outer[0].a == 255 && outer[1].a < 20);
  }
  {  // The miter limit turns a sharp miter into a bevel.
    Px tip[2];
    float limits[2] = { 10.0f, 1.0f };
    for (int i = 0; i < 2; i++) {
      TestContext ctx(16, 16);
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 4.0f;
      ctx.miterLimit = limits[i];
      ctx.beginPath().moveTo(2, 14).lineTo(8, 2).lineTo(14, 14).stroke();
      tip[i] = pixel(ctx, 8, 0);
    }
    CHECK(tip[0].a > 200 && tip[1].a < 20);
  }
  {  // Dashes, their offset, and save()/restore() of the line settings.
    TestContext ctx(16, 8);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 2.0f;
    ctx.setLineDash({ 4.0f, 4.0f });
    ctx.beginPath().moveTo(0, 4).lineTo(16, 4).stroke();
    CHECK(pixel(ctx, 2, 3).a == 255 && pixel(ctx, 6, 3).a == 0 && pixel(ctx, 10, 3).a == 255 && pixel(ctx, 14, 3).a == 0);
    ctx.clearRect(0, 0, 16, 8);
    ctx.lineDashOffset = 4.0f;
    ctx.beginPath().moveTo(0, 4).lineTo(16, 4).stroke();
    CHECK(pixel(ctx, 2, 3).a == 0 && pixel(ctx, 6, 3).a == 255);

    ctx.lineCap = "round";
    ctx.save();
    ctx.lineCap = "square";
    ctx.restore();
    CHECK(ctx.lineCap.get() == LineCap::ROUND);
    ctx.lineCap = "nonsense";  // ignored
    CHECK(ctx.lineCap.get() == LineCap::ROUND);
    ctx.setLineDash({ 3.0f });  // an odd list is repeated
    CHECK(ctx.getLineDash().size() == 2);
    ctx.setLineDash({ -1.0f, 2.0f });  // an invalid one is ignored
    CHECK(ctx.getLineDash().size() == 2 && ctx.getLineDash()[0] == 3.0f);
  }
  {  // Curves: a quadratic and a cubic pass through their midpoints, also under a scale.
    TestContext ctx(16, 16);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 2.0f;
    ctx.beginPath().moveTo(2, 2).quadraticCurveTo(8, 14, 14, 2).stroke();
    CHECK(pixel(ctx, 8, 7).a > 200 && pixel(ctx, 8, 8).a > 200);
    CHECK(pixel(ctx, 8, 4).a == 0 && pixel(ctx, 8, 12).a == 0);

    TestContext ctx2(16, 16);
    ctx2.strokeStyle = "#ffffff";
    ctx2.lineWidth = 2.0f;
    ctx2.scale(2, 2);
    ctx2.lineWidth = 1.0f;
    ctx2.beginPath().moveTo(1, 4).bezierCurveTo(3, 1, 5, 7, 7, 4).stroke();
    CHECK(pixel(ctx2, 8, 7).a > 100 || pixel(ctx2, 8, 8).a > 100);
    CHECK(pixel(ctx2, 8, 1).a == 0 && pixel(ctx2, 8, 14).a == 0);
  }
  {  // A curve with no current point starts at its first control point.
    TestContext ctx(16, 16);
    ctx.fillStyle = "#ffffff";
    ctx.beginPath().bezierCurveTo(2, 2, 14, 2, 14, 14).lineTo(2, 14).closePath().fill();
    CHECK(pixel(ctx, 4, 12).a == 255 && pixel(ctx, 14, 2).a == 0);
  }
  {  // Radial gradient: red in the middle, blue at the edge, padded outside the circle.
    TestContext ctx(16, 16);
    auto g = ctx.createRadialGradient(8, 8, 0, 8, 8, 8);
    g.addColorStop(0.0f, "#ff0000");
    g.addColorStop(1.0f, "#0000ff");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 16);
    Px middle = pixel(ctx, 8, 8), edge = pixel(ctx, 8, 0), corner = pixel(ctx, 0, 0);
    CHECK(middle.r > 200 && middle.b < 60);
    CHECK(edge.b > 200 && edge.r < 60);
    CHECK(corner.b > 240 && corner.r < 15 && corner.a == 255);
  }
  {  // An inner radius pads with the first colour inside it.
    TestContext ctx(16, 16);
    auto g = ctx.createRadialGradient(8, 8, 4, 8, 8, 8);
    g.addColorStop(0.0f, "#ff0000");
    g.addColorStop(1.0f, "#0000ff");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 16);
    Px inner = pixel(ctx, 8, 6), ring = pixel(ctx, 8, 1);
    CHECK(inner.r > 240 && inner.b < 15);
    CHECK(ring.b > 150);
  }
  {  // A PerlinSurface is a source for drawImage(): its noise lands on the context.
    PerlinSurface noise(16, 16, 16, 16, 2, 2, 2, 8.0f);
    auto expected = noise.createImage(1.0f);
    TestContext ctx(16, 16);
    ctx.drawImage(noise, 0, 0, 16, 16);
    int distinct = 0, worst = 0;
    int seen[256] = { 0 };
    for (unsigned y = 0; y < 16; y++)
      for (unsigned x = 0; x < 16; x++) {
	Px p = pixel(ctx, x, y);
	int want = expected->getData().getData()[4 * (y * 16 + x)];
	CHECK(p.a == 255);
	worst = std::max(worst, std::abs(p.r - want));
	if (!seen[p.r]++) distinct++;
      }
    CHECK(distinct > 20);  // not flat
    CHECK(worst <= 24);    // and the same picture
  }
  for (float scale : { 1.25f, 1.5f }) {  // An exact pixel size at a fractional scale.
    TestContext ctx(PixelSize{ 101, 37 }, 4, scale);
    CHECK(ctx.getActualWidth() == 101 && ctx.getActualHeight() == 37);
    CHECK(near(ctx.getWidth(), 101 / scale) && near(ctx.getHeight(), 37 / scale));
    auto img = ctx.getDefaultSurface().createImage(scale);
    CHECK(img->getData().getWidth() == 101 && img->getData().getHeight() == 37);
    auto surface = ctx.createSurface(PixelSize{ 101, 37 }, 4);
    CHECK(surface->getActualWidth() == 101 && surface->getActualHeight() == 37);
    CHECK(near(surface->getLogicalWidth(), 101 / scale));
    ctx.resize(PixelSize{ 50, 31 });
    CHECK(ctx.getActualWidth() == 50 && ctx.getActualHeight() == 31);
    CHECK(near(ctx.getWidth(), 50 / scale) && near(ctx.getHeight(), 31 / scale));
    TestFactory factory(scale, nullptr);
    auto from_factory = factory.createContext(PixelSize{ 101, 37 }, 4);
    CHECK(from_factory->getActualWidth() == 101 && from_factory->getActualHeight() == 37);
    CHECK(factory.createSurface(PixelSize{ 7, 3 }, 4)->getActualWidth() == 7);
    // An Image resized to a fractional logical size gets the same pixels.
    Image image(scale);
    image.getDataPtr() = std::make_unique<ImageData>(4, 4, 4);
    image.scale(101 / scale, 37 / scale);
    CHECK(image.getData().getWidth() == 101 && image.getData().getHeight() == 37);
  }
  {  // The size in logical units still rounds the pixels down, as before.
    TestContext ctx(81, 30, 4, 1.25f);
    CHECK(ctx.getActualWidth() == 101 && ctx.getActualHeight() == 37 && ctx.getWidth() == 81);
  }
  {  // Logical coordinates land on the right pixels at a fractional scale:
     // at 1.25, logical 4..12 is device 5..15, and the logical size reaches
     // the last pixel.
    TestContext ctx(PixelSize{ 101, 37 }, 4, 1.25f);
    ctx.fillStyle = "#ff0000";
    ctx.fillRect(4, 4, 8, 8);
    CHECK(pixel(ctx, 5, 5).a == 255 && pixel(ctx, 14, 14).a == 255);
    CHECK(pixel(ctx, 4, 5).a == 0 && pixel(ctx, 5, 4).a == 0 && pixel(ctx, 15, 14).a == 0 && pixel(ctx, 14, 15).a == 0);
    ctx.fillStyle = "#0000ff";
    ctx.fillRect(ctx.getWidth() - 0.8, ctx.getHeight() - 0.8, 0.8, 0.8);  // the last pixel
    CHECK(pixel(ctx, 100, 36).b == 255 && pixel(ctx, 100, 36).a == 255);
    CHECK(pixel(ctx, 99, 36).a == 0 && pixel(ctx, 100, 35).a == 0);
  }
  {  // drawImage() of a pixel-sized surface at its logical size copies it 1:1.
    TestContext src(PixelSize{ 10, 10 }, 4, 1.25f);
    src.fillStyle = "#00ff00";
    src.fillRect(0, 0, src.getWidth(), src.getHeight());
    TestContext ctx(PixelSize{ 101, 37 }, 4, 1.25f);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(src, 4, 4, src.getWidth(), src.getHeight());
    CHECK(pixel(ctx, 5, 5).g == 255 && pixel(ctx, 14, 14).g == 255 && pixel(ctx, 14, 14).a == 255);
    CHECK(pixel(ctx, 4, 5).a == 0 && pixel(ctx, 15, 14).a == 0 && pixel(ctx, 14, 15).a == 0);
  }
  {  // A shadow on a pixel-sized surface is offset in logical units.
    TestContext ctx(PixelSize{ 101, 37 }, 4, 1.25f);
    ctx.shadowColor = Color(1.0f, 0.0f, 0.0f, 1.0f);
    ctx.shadowOffsetX = 8;  // 10 px
    ctx.fillStyle = "#0000ff";
    ctx.fillRect(4, 4, 4, 4);  // device 5..10
    Px shadow = pixel(ctx, 17, 7), shape = pixel(ctx, 7, 7);
    CHECK(shadow.r > 200 && shadow.b < 50);
    CHECK(shape.b == 255);
    CHECK(pixel(ctx, 22, 7).a < 50);
  }
  for (float scale : { 1.0f, 1.5f, 2.0f }) {  // lineWidth 0 is a one-pixel hairline at any scale.
    TestContext ctx(PixelSize{ 16, 16 }, 4, scale);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 0.0f;
    // On a device pixel centre it is one crisp row.
    ctx.beginPath().moveTo(0, 5.5 / scale).lineTo(16, 5.5 / scale).stroke();
    CHECK(pixel(ctx, 4, 5).a == 255 && pixel(ctx, 4, 4).a == 0 && pixel(ctx, 4, 6).a == 0);
    // Anywhere else it still covers one pixel in total: device 9.75..10.75.
    ctx.beginPath().moveTo(10.25 / scale, 0).lineTo(10.25 / scale, 16).stroke();
    CHECK(close_to(pixel(ctx, 9, 12).a, 64, 3) && close_to(pixel(ctx, 10, 12).a, 191, 3));
    CHECK(pixel(ctx, 8, 12).a == 0 && pixel(ctx, 11, 12).a == 0);
    // A normal line scales with the display.
    TestContext wide(PixelSize{ 16, 16 }, 4, scale);
    wide.strokeStyle = "#ffffff";
    wide.lineWidth = 1.0f;
    wide.beginPath().moveTo(8 / scale, 0).lineTo(8 / scale, 16).stroke();
    CHECK(close_to(pixel(wide, 7, 8).a + pixel(wide, 8, 8).a, int(255 * scale), 3));
    CHECK(pixel(wide, 6, 8).a == 0 && pixel(wide, 9, 8).a == 0);
  }
  {  // A hairline ignores a scaling transform; a normal line follows it.
    for (float scale : { 1.0f, 2.0f }) {
      TestContext ctx(PixelSize{ 32, 32 }, 4, scale);
      ctx.strokeStyle = "#ffffff";
      ctx.scale(4, 4);
      ctx.lineWidth = 0.0f;
      ctx.beginPath().moveTo(0, 5.5 / 4 / scale).lineTo(8, 5.5 / 4 / scale).stroke();
      CHECK(pixel(ctx, 10, 5).a == 255 && pixel(ctx, 10, 4).a == 0 && pixel(ctx, 10, 6).a == 0);
      ctx.lineWidth = 1.0f;  // 4 * scale px wide
      ctx.beginPath().moveTo(0, 20.0 / 4 / scale).lineTo(8, 20.0 / 4 / scale).stroke();
      int rows = 0;
      for (unsigned y = 10; y < 32; y++) rows += pixel(ctx, 10, y).a == 255;
      CHECK(rows == int(4 * scale));
      // The line settings still apply: a dashed hairline has gaps.
      ctx.setLineDash({ 1.0f, 1.0f });
      ctx.lineWidth = 0.0f;
      ctx.beginPath().moveTo(0.5 / 4 / scale, 0).lineTo(0.5 / 4 / scale, 8).stroke();
      CHECK(pixel(ctx, 0, int(1 * scale)).a == 255 && pixel(ctx, 0, int(6 * scale)).a == 0);
      CHECK(pixel(ctx, 1, int(1 * scale)).a == 0);
    }
  }
  {  // A hairline strokes text too, and the same stroke at lineWidth 1 is no thinner.
    TestContext thin(64, 32, 4, 2.0f), thick(64, 32, 4, 2.0f);
    for (auto * ctx : { &thin, &thick }) {
      ctx->strokeStyle = "#ffffff";
      ctx->font.size = 24;
      ctx->lineWidth = ctx == &thin ? 0.0f : 1.0f;
      ctx->strokeText("W", 4, 24);
    }
    int a = 0, b = 0;
    for (unsigned x = 0; x < 128; x++) { a += columnCoverage(thin, x); b += columnCoverage(thick, x); }
    CHECK(a > 0 && a <= b);
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
