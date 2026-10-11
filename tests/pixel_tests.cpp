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
    // Integer coordinates are pixel edges, as in HTML5 canvas.
    CHECK(pixel(ctx, 2, 2).a == 255 && pixel(ctx, 5, 5).a == 255);
    CHECK(pixel(ctx, 1, 2).a == 0 && pixel(ctx, 6, 5).a == 0 && pixel(ctx, 2, 1).a == 0 && pixel(ctx, 5, 6).a == 0);
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
  {  // A 1 px line on an integer coordinate straddles two rows at half
     // coverage; on a .5 coordinate it is one crisp row (HTML5 canvas).
    ContextCairo ctx(8, 8);
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 1.0f;
    ctx.beginPath().moveTo(0, 2).lineTo(8, 2).stroke();
    ctx.beginPath().moveTo(0, 5.5).lineTo(8, 5.5).stroke();
    CHECK(near(pixel(ctx, 4, 1).a, 128) && near(pixel(ctx, 4, 2).a, 128));
    CHECK(pixel(ctx, 4, 5).a == 255 && pixel(ctx, 4, 4).a == 0 && pixel(ctx, 4, 6).a == 0);
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
    auto g = ctx.createLinearGradient(0, 0, 16, 0);
    g.addColorStop(0.0f, "#000000");
    g.addColorStop(1.0f, "#ffffff");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 16, 2);
    Px a = pixel(ctx, 1, 0), b = pixel(ctx, 8, 0), c = pixel(ctx, 14, 0);
    CHECK(a.r < b.r && b.r < c.r);
    CHECK(a.r < 40 && c.r > 215);
  }
  {  // An image drawn onto a context reads back where it was placed.
    ContextCairo ctx(8, 8);
    unsigned char rgba[4 * 2 * 2];
    for (int i = 0; i < 4; i++) { rgba[4 * i] = 0; rgba[4 * i + 1] = 0; rgba[4 * i + 2] = 255; rgba[4 * i + 3] = 255; }
    ImageData img(rgba, 2, 2, 4);
    ctx.drawImage(img, 4, 4, 2, 2);
    for (unsigned y = 4; y < 6; y++)
      for (unsigned x = 4; x < 6; x++) {
        Px in = pixel(ctx, x, y);
        CHECK(in.a == 255 && in.r == 255 && in.g == 0);
      }
    CHECK(pixel(ctx, 3, 4).a == 0 && pixel(ctx, 6, 5).a == 0);
    CHECK(pixel(ctx, 1, 1).a == 0);
  }
  {  // Each gradient is its own: a second one doesn't change the first.
    ContextCairo ctx(16, 2);
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
    ContextCairo ctx(8, 8);
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
    ContextCairo ctx(8, 8);
    ctx.fillStyle("#0000ff").translate(2, 2).beginPath().rect(0, 0, 2, 2).fill();
    CHECK(pixel(ctx, 3, 3).b == 255 && pixel(ctx, 1, 1).a == 0);
    ctx.lineWidth(2.0f).strokeStyle(Color(1.0f, 0.0f, 0.0f, 1.0f)).resetTransform().beginPath().moveTo(0, 7).lineTo(8, 7).stroke();
    CHECK(pixel(ctx, 4, 7).r == 255);
  }
  {  // Assigning a style from another context keeps the owner.
    ContextCairo a(4, 4), b(4, 4);
    b.fillStyle = "#ff0000";
    a.fillStyle = b.fillStyle;
    a.fillStyle("#00ff00").fillRect(0, 0, 4, 4);
    CHECK(pixel(a, 1, 1).g == 255 && pixel(b, 1, 1).a == 0);
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
