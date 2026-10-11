#ifndef _CANVAS_FLOYDSTEINBERG_H_
#define _CANVAS_FLOYDSTEINBERG_H_

#include <InternalFormat.h>
#include <memory>

namespace canvas {
  class ImageData;

  class FloydSteinberg  {
  public:
    FloydSteinberg(InternalFormat _target_format) : target_format(_target_format) { }

    unsigned int apply(const ImageData & input_image, unsigned char * output, unsigned int bytesPerRow) const;

  private:
    InternalFormat target_format;
  };
};

#endif
