#include <canvas/ColorSpace.h>

#include <algorithm>
#include <cmath>

namespace canvas {
  namespace {
    inline float finite(float x) { return std::isfinite(x) ? x : 0.0f; }

    // Linear Rec.2020 to linear Rec.709 (the primaries of sRGB), D65.
    void toLinearSrgb(const float in[3], float out[3]) {
      out[0] = 1.6604910f * in[0] - 0.5876411f * in[1] - 0.0728499f * in[2];
      out[1] = -0.1245505f * in[0] + 1.1328999f * in[1] - 0.0083494f * in[2];
      out[2] = -0.0181508f * in[0] - 0.1005789f * in[1] + 1.1187297f * in[2];
    }

    // OKLab on linear Rec.709 light.
    void toOklab(const float rgb[3], float lab[3]) {
      float l = 0.4122214708f * rgb[0] + 0.5363325363f * rgb[1] + 0.0514459929f * rgb[2];
      float m = 0.2119034982f * rgb[0] + 0.6806995451f * rgb[1] + 0.1073969566f * rgb[2];
      float s = 0.0883024619f * rgb[0] + 0.2817188376f * rgb[1] + 0.6299787005f * rgb[2];
      float l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
      lab[0] = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
      lab[1] = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
      lab[2] = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
    }

    void fromOklab(const float lab[3], float rgb[3]) {
      float l_ = lab[0] + 0.3963377774f * lab[1] + 0.2158037573f * lab[2];
      float m_ = lab[0] - 0.1055613458f * lab[1] - 0.0638541728f * lab[2];
      float s_ = lab[0] - 0.0894841775f * lab[1] - 1.2914855480f * lab[2];
      float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
      rgb[0] = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
      rgb[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
      rgb[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    }

    // Pulls linear sRGB `v` into the gamut (every component at least 0): chroma
    // reduced at the colour's own lightness and hue.
    void reduceChroma(float v[3]) {
      float lab[3];
      toOklab(v, lab);
      if (!(lab[0] > 0.0f)) {
        v[0] = v[1] = v[2] = 0.0f;
        return;
      }
      float lo = 0.0f, hi = 1.0f;
      for (int i = 0; i < 20; i++) {
	float mid = 0.5f * (lo + hi);
	float scaled[3] = { lab[0], lab[1] * mid, lab[2] * mid }, rgb[3];
	fromOklab(scaled, rgb);
	if (std::min({ rgb[0], rgb[1], rgb[2] }) >= 0.0f) lo = mid;
	else hi = mid;
      }
      float scaled[3] = { lab[0], lab[1] * lo, lab[2] * lo };
      fromOklab(scaled, v);
      for (int i = 0; i < 3; i++) v[i] = std::max(0.0f, v[i]);
    }

    // Light past white: the brightest component is scaled to 1 and the colour
    // drifts toward white by how much was taken away, 1 - 1/peak (Khronos PBR
    // Neutral with its knee at 1, so SDR white stays white).
    void toneMap(float rgb[3]) {
      float peak = std::max({ rgb[0], rgb[1], rgb[2] });
      if (!(peak > 1.0f)) return;
      float drift = 1.0f - 1.0f / peak;
      for (int i = 0; i < 3; i++) {
	float c = rgb[i] / peak;
	rgb[i] = c + (1.0f - c) * drift;
      }
    }

    float encode(float x) {
      x = std::clamp(x, 0.0f, 1.0f);
      return x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
    }
  }

  Color linearRec2020ToSrgb(const Color & c) {
    float in[3] = { finite(c.red), finite(c.green), finite(c.blue) }, v[3];
    toLinearSrgb(in, v);
    if (std::min({ v[0], v[1], v[2] }) < 0.0f) reduceChroma(v);
    toneMap(v);
    return Color(encode(v[0]), encode(v[1]), encode(v[2]), c.alpha);
  }
}
