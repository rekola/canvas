// Pixel tests for what a client draws, run on whichever backend the build
// has (Cairo, else GDI+). The surface's memory is read back through
// Surface::createImage().
#if defined(CANVAS_HAS_CAIRO)
#include <ContextCairo.h>
namespace canvas { typedef ContextCairo TestContext; typedef CairoContextFactory TestFactory; }
#elif defined(CANVAS_HAS_GDIPLUS)
#include <ContextGDIPlus.h>
namespace canvas { typedef ContextGDIPlus TestContext; typedef GDIPlusContextFactory TestFactory; }
#else
#error "pixel tests need the Cairo or the GDI+ backend"
#endif

#include <cstdio>
#include <exception>
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
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
