// Backend-independent palette and dithering tests.
#include <canvas/Dither.h>
#include <canvas/ImageData.h>
#include <canvas/Palette.h>

#include <cstdio>
#include <cstdlib>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static ImageData grey(unsigned w, unsigned h, unsigned char v) {
  ImageData img(w, h, 3);
  for (size_t i = 0; i < size_t(w) * h; i++) img.getData()[3 * i] = img.getData()[3 * i + 1] = img.getData()[3 * i + 2] = v;
  return img;
}

static Palette blackAndWhite() {
  return Palette({ { 0, 0, 0 }, { 255, 255, 255 } });
}

static size_t whites(const std::vector<unsigned char> & indices) {
  size_t n = 0;
  for (auto v : indices) n += v;
  return n;
}

int main() {
  {  // The xterm palette: layout and values by xterm colour number.
    Palette p = Palette::xterm256();
    CHECK(p.size() == 256);
    CHECK(p[0].red == 0 && p[15].red == 255 && p[15].green == 255 && p[9].red == 255 && p[9].green == 0);
    CHECK(p[16].red == 0 && p[16].blue == 0 && p[231].red == 255 && p[231].blue == 255);
    CHECK(p[196].red == 255 && p[196].green == 0 && p[196].blue == 0);
    CHECK(p[21].blue == 255 && p[21].red == 0 && p[17].blue == 95);
    CHECK(p[232].red == 8 && p[255].red == 238 && p[255].green == 238);
  }
  {  // nearest(): the closest entry, the lowest index on a tie, and the first-index limit.
    Palette p = Palette::xterm256();
    CHECK(p.nearest(250, 250, 250) == 15);
    CHECK(p.nearest(10, 10, 10) == 232);
    CHECK(p.nearest(255, 0, 0) == 9);
    CHECK(p.nearest(255, 0, 0, 16) == 196);
    CHECK(p.nearest(0, 0, 0) == 0 && p.nearest(0, 0, 0, 16) == 16);
    CHECK(p.nearest(0, 0, 90) == 17);
    CHECK(blackAndWhite().nearest(100, 100, 100) == 0 && blackAndWhite().nearest(200, 200, 200) == 1);
    CHECK(Palette().nearest(1, 2, 3) == 0);
  }
  {  // More than 256 entries are cut off.
    std::vector<Palette::Entry> many;
    for (int i = 0; i < 300; i++) many.push_back(Palette::Entry{ (unsigned char)i, 0, 0 });
    CHECK(Palette(many).size() == 256);
  }
  {  // spacing(): 255 for black and white, around the cube step for xterm.
    CHECK(blackAndWhite().spacing() == 255);
    int s = Palette::xterm256().spacing();
    CHECK(s >= 30 && s <= 60);
  }
  {  // Exact palette colours come out as themselves, with every method.
    Palette p = Palette::xterm256();
    unsigned char colours[5][3] = { { 255, 0, 0 }, { 0, 135, 215 }, { 95, 95, 95 }, { 255, 255, 255 }, { 28, 28, 28 } };
    ImageData img(8, 8, 3);
    for (int i = 0; i < 64; i++) for (int c = 0; c < 3; c++) img.getData()[3 * i + c] = colours[(i * 7) % 5][c];
    DitherMethod methods[3] = { DitherMethod::NONE, DitherMethod::FLOYD_STEINBERG, DitherMethod::ORDERED };
    for (auto method : methods) {
      DitherOptions o;
      o.method = method;
      o.strength = 1;  // so ordered dithering can't move an exact colour
      auto out = quantize(img, p, o);
      bool same = out.size() == 64;
      for (int i = 0; i < 64 && same; i++) {
	const auto & e = p[out[i]];
	for (int c = 0; c < 3; c++) {
	  unsigned char got = c == 0 ? e.red : c == 1 ? e.green : e.blue;
	  if (got != colours[(i * 7) % 5][c]) same = false;
	}
      }
      CHECK(same);
    }
  }
  {  // The density of a dithered grey follows its level, with either method.
    Palette bw = blackAndWhite();
    for (unsigned char level : { 32, 64, 128, 192 }) {
      ImageData img = grey(32, 32, level);
      DitherOptions o;
      o.method = DitherMethod::ORDERED;
      double ordered = whites(quantize(img, bw, o)) / 1024.0;
      o.method = DitherMethod::FLOYD_STEINBERG;
      double diffused = whites(quantize(img, bw, o)) / 1024.0;
      double want = level / 255.0;
      CHECK(std::abs(ordered - want) < 0.03);
      CHECK(std::abs(diffused - want) < 0.03);
    }
    // Without dithering it is a plain threshold.
    DitherOptions o;
    o.method = DitherMethod::NONE;
    CHECK(whites(quantize(grey(8, 8, 100), bw, o)) == 0 && whites(quantize(grey(8, 8, 200), bw, o)) == 64);
  }
  {  // Ordered dithering is stable: changing one pixel changes only that pixel's
     // output. Error diffusion carries the change along.
    Palette bw = blackAndWhite();
    ImageData a = grey(16, 16, 128), b = grey(16, 16, 128);
    for (int c = 0; c < 3; c++) b.getData()[3 * (3 * 16 + 3) + c] = 0;
    DitherOptions o;
    o.method = DitherMethod::ORDERED;
    auto oa = quantize(a, bw, o), ob = quantize(b, bw, o);
    int ordered_changes = 0;
    for (size_t i = 0; i < oa.size(); i++) ordered_changes += oa[i] != ob[i];
    CHECK(ordered_changes <= 1);
    o.method = DitherMethod::FLOYD_STEINBERG;
    auto fa = quantize(a, bw, o), fb = quantize(b, bw, o);
    int diffused_changes = 0;
    for (size_t i = 0; i < fa.size(); i++) diffused_changes += fa[i] != fb[i];
    CHECK(diffused_changes > 1);
  }
  {  // The Bayer matrix sizes all work, and a bigger one has finer levels.
    Palette bw = blackAndWhite();
    ImageData img = grey(16, 16, 100);
    for (int n : { 2, 4, 8, 16 }) {
      DitherOptions o;
      o.matrixSize = n;
      double d = whites(quantize(img, bw, o)) / 256.0;
      CHECK(std::abs(d - 100 / 255.0) < (n == 2 ? 0.15 : 0.06));
    }
  }
  {  // Translucent pixels are composited over the background by the image's own format.
    Palette bw = blackAndWhite();
    DitherOptions o;
    o.method = DitherMethod::NONE;
    o.backgroundRed = o.backgroundGreen = o.backgroundBlue = 255;
    unsigned char straight[4] = { 0, 0, 0, 0 };  // transparent black, straight RGBA
    ImageData a(straight, 1, 1, 4);
    CHECK(quantize(a, bw, o)[0] == 1);  // the white background shows
    unsigned char premultiplied[4] = { 255, 255, 255, 255 };  // opaque white, BGRA premultiplied
    ImageData b(premultiplied, 1, 1, 4);
    b.setFormat(ChannelOrder::BGR, true);
    CHECK(quantize(b, bw, o)[0] == 1);
    unsigned char half[4] = { 0, 0, 0, 128 };  // half-transparent black over white is mid grey
    ImageData c(half, 1, 1, 4);
    DitherOptions d;
    d.method = DitherMethod::NONE;
    d.backgroundRed = d.backgroundGreen = d.backgroundBlue = 255;
    CHECK(quantize(c, Palette({ { 0, 0, 0 }, { 127, 127, 127 }, { 255, 255, 255 } }), d)[0] == 1);
    o.backgroundRed = o.backgroundGreen = o.backgroundBlue = 0;
    CHECK(quantize(a, bw, o)[0] == 0);  // over black
  }
  {  // The channel order is honoured: BGR red is red.
    Palette p = Palette::xterm256();
    unsigned char bgr[3] = { 0, 0, 255 };
    ImageData img(bgr, 1, 1, 3);
    img.setFormat(ChannelOrder::BGR, false);
    DitherOptions o;
    o.method = DitherMethod::NONE;
    CHECK(quantize(img, p, o)[0] == 9);
  }
  {  // firstIndex keeps every output in range, and an empty palette or image is harmless.
    Palette p = Palette::xterm256();
    DitherOptions o;
    o.firstIndex = 16;
    bool ok = true;
    for (auto v : quantize(grey(16, 16, 77), p, o)) if (v < 16) ok = false;
    CHECK(ok);
    CHECK(quantize(grey(2, 2, 5), Palette()).size() == 4);
    CHECK(quantize(ImageData(), p).empty());
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
