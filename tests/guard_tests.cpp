// A client with its own Color.h and Image.h (generic guard names) must be
// able to include canvas headers after them. The headers live in canvas/,
// so the names can't collide in the include path either.
#ifndef _COLOR_H_
#define _COLOR_H_
class Color { public: int r = 1; };
#endif
#ifndef _IMAGE_H_
#define _IMAGE_H_
class Image { public: int w = 2; };
#endif

#include <canvas/ContextCairo.h>

int main() {
  ::Color own;
  ::Image own_image;
  canvas::Color c(0.0f, 0.5f, 1.0f, 1.0f);
  return (own.r == 1 && own_image.w == 2 && c.green == 0.5f) ? 0 : 1;
}
