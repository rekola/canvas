#ifndef _CANVAS_HALF_H_
#define _CANVAS_HALF_H_

#include <cstdint>
#include <cstring>

namespace canvas {
  // IEEE 754 binary16 <-> binary32, for backends whose float surface is FP16
  // (the Direct2D render target). ImageData itself only holds 32-bit floats,
  // so such a backend widens on read-back and narrows on upload. Values
  // above 65504 become infinity and values below about 6e-8 become zero.
  inline float halfToFloat(uint16_t h) {
    uint32_t sign = uint32_t(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1fu, mant = h & 0x3ffu, bits;
    if (exp == 0) {
      if (mant == 0) {
	bits = sign;
      } else {
	// Subnormal half: normalize it.
	exp = 113;
	while (!(mant & 0x400u)) { mant <<= 1; exp--; }
	bits = sign | (exp << 23) | ((mant & 0x3ffu) << 13);
      }
    } else if (exp == 31) {
      bits = sign | 0x7f800000u | (mant << 13);
    } else {
      bits = sign | ((exp + 112) << 23) | (mant << 13);
    }
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
  }

  // Rounds to nearest even.
  inline uint16_t floatToHalf(float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    uint32_t sign = (x >> 16) & 0x8000u, abs = x & 0x7fffffffu;
    if (abs >= 0x7f800000u) return uint16_t(sign | 0x7c00u | (abs > 0x7f800000u ? 0x200u : 0u));
    if (abs >= 0x477ff000u) return uint16_t(sign | 0x7c00u);  // rounds to infinity
    if (abs < 0x38800000u) {
      // Subnormal half or zero.
      if (abs < 0x33000000u) return uint16_t(sign);
      uint32_t mant = (abs & 0x7fffffu) | 0x800000u;
      int shift = 126 - int(abs >> 23);
      uint32_t half = mant >> shift, rem = mant & ((1u << shift) - 1), mid = 1u << (shift - 1);
      if (rem > mid || (rem == mid && (half & 1))) half++;
      return uint16_t(sign | half);
    }
    uint32_t half = ((abs - 0x38000000u) >> 13);
    uint32_t rem = abs & 0x1fffu;
    if (rem > 0x1000u || (rem == 0x1000u && (half & 1))) half++;
    return uint16_t(sign | half);
  }
};

#endif
