#ifndef _CANVAS_COLORPROVIDER_H_
#define _CANVAS_COLORPROVIDER_H_

#include "Color.h"

#include <vector>

namespace canvas {
  class ColorProvider {
  public:
    enum ColorStyle {
      COLD = 1,
      CHART,
      CHART2
    };
    ColorProvider(ColorStyle _style);

    const Color & getNextColor() {
      if (colors.empty()) {
	return null_color;
      } else {
	return colors[(cursor++) % colors.size()];
      }
    }

    const Color & getColorByIndex(int indx) {
      if (colors.empty()) {
	return null_color;
      } else {
	return colors[indx % colors.size()];
      }
    }

    void reset() { cursor = 0; }
  
  private:
    ColorStyle style;
    int cursor = 0;
    std::vector<Color> colors;
    Color null_color;
  };
};

#endif
