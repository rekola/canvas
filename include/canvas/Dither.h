#ifndef _CANVAS_DITHER_H_
#define _CANVAS_DITHER_H_

#include "Palette.h"

#include <cstddef>
#include <vector>

namespace canvas {
  class ImageData;

  enum class DitherMethod {
    NONE,             // the nearest palette entry of each pixel
    FLOYD_STEINBERG,  // error diffusion: smooth, but one changed pixel can change everything after it
    ORDERED           // Bayer matrix: each pixel is decided by itself and its position, so an animation doesn't shimmer
  };

  struct DitherOptions {
    DitherMethod method = DitherMethod::ORDERED;
    // The side of the Bayer matrix: 2, 4, 8 or 16.
    int matrixSize = 8;
    // Ordered dithering: how far a threshold moves a channel, 0 to 255.
    // 0 takes the palette's spacing().
    int strength = 0;
    // Translucent pixels are composited over this colour first.
    unsigned char backgroundRed = 0, backgroundGreen = 0, backgroundBlue = 0;
    // Leave out the entries before this index (16 skips the xterm system
    // colours, which terminals let the user change).
    size_t firstIndex = 0;
  };

  // Quantizes the image to palette indices, one byte per pixel, rows top to
  // bottom without padding (width * height bytes). The image is read by its
  // format (ImageData::getChannelOrder(), isPremultiplied()). One channel is
  // taken as a grey level here, not as an alpha mask. Float images are
  // clamped to [0, 1] and scaled to bytes as they are. An empty palette
  // gives all zeros.
  std::vector<unsigned char> quantize(const ImageData & image, const Palette & palette, const DitherOptions & options = DitherOptions());
};

#endif
