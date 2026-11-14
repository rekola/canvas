#ifndef _CANVAS_COLORSPACE_H_
#define _CANVAS_COLORSPACE_H_

#include "Color.h"

namespace canvas {
  // What the Colors given to a Context mean.
  enum class ColorSpace {
    // Components are drawn as given (the default; 8-bit surfaces clamp them to [0, 1]).
    AS_GIVEN,
    // Linear-light Rec.2020 primaries, 1.0 being SDR white; above 1 is HDR.
    // A FLOAT32 surface still draws the numbers as given (the output is linear
    // Rec.2020 float). An 8-bit surface maps every colour it is given to sRGB
    // first (linearRec2020ToSrgb()) and blends in the encoded values, as always.
    LINEAR_REC2020
  };

  // What an 8-bit sRGB surface can show of a linear Rec.2020 colour, as encoded
  // sRGB components in [0, 1] with the alpha unchanged. A colour outside the
  // sRGB gamut has its chroma reduced at its own OKLab lightness and hue. Light
  // past white (a component above 1 in linear sRGB) is compressed toward 1 and
  // drifts toward white the more of it there is, so a very bright colour has a
  // pale core instead of a flat plateau of its own hue (Khronos PBR Neutral's
  // compression, started at 1 so SDR white stays white). Dark colours are left
  // as they are.
  Color linearRec2020ToSrgb(const Color & c);
};

#endif
