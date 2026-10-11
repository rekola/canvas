// Pixel tests for what a client draws. The Cairo surface's memory is
// read back through Surface::createImage().
#include <ContextCairo.h>

#include <cstdio>
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

static bool near(int a, int b, int tol = 2) { return std::abs(a - b) <= tol; }

int main() {
  {  // A filled rectangle: exact colour inside, untouched outside.
    ContextCairo ctx(8, 8);
    ctx.fillStyle = "#ff0000";
    ctx.fillRect(2, 2, 4, 4);
    Px in = pixel(ctx, 3, 3), out = pixel(ctx, 0, 0);
    CHECK(in.r == 255 && in.g == 0 && in.b == 0 && in.a == 255);
    CHECK(out.a == 0 && out.r == 0);
  }
  {  // Premultiplied alpha: 50% white is stored as 128 in every channel.
    ContextCairo ctx(4, 4);
    ctx.fillStyle = Color(1.0f, 1.0f, 1.0f, 0.5f);
    ctx.fillRect(0, 0, 4, 4);
    Px p = pixel(ctx, 1, 1);
    CHECK(near(p.a, 128) && near(p.r, 128) && near(p.g, 128) && near(p.b, 128));
  }
  {  // An antialiased diagonal line has partial coverage next to its centre
     // and nothing far from it.
    ContextCairo ctx(16, 16);
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
  {  // A polyline covers each of its segments.
    ContextCairo ctx(16, 16);
    ctx.strokeStyle = "#00ff00";
    ctx.lineWidth = 2.0f;
    ctx.beginPath().moveTo(1, 8).lineTo(8, 8).lineTo(8, 15).stroke();
    CHECK(pixel(ctx, 4, 8).g > 200);
    CHECK(pixel(ctx, 8, 12).g > 200);
    CHECK(pixel(ctx, 12, 3).a == 0);
  }
  {  // A linear gradient runs monotonically between its stops.
    ContextCairo ctx(16, 2);
    auto & g = ctx.createLinearGradient(0, 0, 16, 0);
    g.addColorStop(0.0f, "#000000");
    g.addColorStop(1.0f, "#ffffff");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 2);
    // Row 1: paths are offset by half a pixel, so row 0 is half covered.
    Px a = pixel(ctx, 1, 1), b = pixel(ctx, 8, 1), c = pixel(ctx, 14, 1);
    CHECK(a.r < b.r && b.r < c.r);
    CHECK(a.r < 40 && c.r > 215);
  }
  {  // An image drawn onto a context reads back where it was placed.
    ContextCairo ctx(8, 8);
    unsigned char rgba[4 * 2 * 2];
    for (int i = 0; i < 4; i++) { rgba[4 * i] = 0; rgba[4 * i + 1] = 0; rgba[4 * i + 2] = 255; rgba[4 * i + 3] = 255; }
    ImageData img(rgba, 2, 2, 4);
    ctx.drawImage(img, 4, 4, 2, 2);
    // Images are drawn half a pixel off too, so only the middle is exact.
    Px in = pixel(ctx, 5, 5);
    CHECK(in.a == 255 && in.r == 255 && in.g == 0);
    CHECK(pixel(ctx, 1, 1).a == 0);
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
