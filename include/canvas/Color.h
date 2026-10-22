#ifndef _CANVAS_COLOR_H_
#define _CANVAS_COLOR_H_

#include <string>

namespace canvas {
  // Red, green, blue and alpha as floats. Strings give 0 to 1 values, and
  // the components may be set to anything: a float surface (PixelType::FLOAT32)
  // draws the numbers it is given, unclamped.
  //
  // Canvas does not interpret the colour space. It blends, anti-aliases and
  // interpolates gradients by arithmetic on the components, and applies no
  // transfer function, primaries conversion or tone mapping. A client that
  // wants physically correct blending supplies linear-light values (for
  // example linear Rec.2020 with 1.0 as SDR white, where HDR is above 1);
  // gamma-encoded values blend as gamma-encoded numbers. Alpha is never a
  // colour: it is a coverage in [0, 1]. Colours are straight (not
  // premultiplied); gradient stops are interpolated in that straight form.
  // 8-bit surfaces clamp to [0, 1] as always.
  class Color {
  public:
    static Color BLACK, WHITE, RED;
    
  Color() : red(0.0f), green(0.0f), blue(0.0f), alpha(1.0f) { }
    Color(const std::string & s)
      : red(0.0f), green(0.0f), blue(0.0f), alpha(1.0f) {
      setValue(s);
    }
  Color(float _red, float _green, float _blue, float _alpha)
    : red(_red), green(_green), blue(_blue), alpha(_alpha) { }
    
    Color & operator=(const std::string & s);
    
	Color mix(float f, const Color & other) {
		return Color(f * other.red + (1 - f) * red,
			f * other.green + (1 - f) * green,
			f * other.blue + (1 - f) * blue,
			f * other.alpha + (1 - f) * alpha);
	}
    float red, green, blue, alpha;
    
  private:
    void setValue(const std::string & s);
  };

  // shim for Attribute<Color>
  inline void assignValue(Color & a, const std::string & b) { a = b; }
};

#endif
