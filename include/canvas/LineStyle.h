#ifndef _CANVAS_LINESTYLE_H_
#define _CANVAS_LINESTYLE_H_

#include "Attribute.h"

#include <cstring>
#include <string>
#include <vector>

namespace canvas {
  enum class LineCap { BUTT, ROUND, SQUARE };
  enum class LineJoin { MITER, ROUND, BEVEL };

  // The stroke settings a backend needs besides the width: what
  // Surface::renderPath() gets. The dash lengths and offset are in user
  // units (the backend scales them with the transform).
  struct LineStyle {
    LineCap cap = LineCap::BUTT;
    LineJoin join = LineJoin::MITER;
    float miterLimit = 10.0f;
    float dashOffset = 0.0f;
    std::vector<float> dash;
  };

  class LineCapAttribute : public AttributeBase {
  public:
    LineCapAttribute(GraphicsState * _context, LineCap _value = LineCap::BUTT) : AttributeBase(_context), value(_value) { }
    LineCapAttribute(const LineCapAttribute & other) : AttributeBase(other), value(other.value) { }

    LineCapAttribute & operator=(const LineCapAttribute & other) { value = other.value; return *this; }
    LineCapAttribute & operator=(LineCap other) { value = other; return *this; }
    LineCapAttribute & operator=(const std::string & s) { setValue(s.c_str()); return *this; }
    LineCapAttribute & operator=(const char * s) { setValue(s); return *this; }
    Context & operator()(LineCap other) { value = other; return getContext(); }
    Context & operator()(const std::string & s) { setValue(s.c_str()); return getContext(); }
    Context & operator()(const char * s) { setValue(s); return getContext(); }

    LineCap get() const { return value; }

  private:
    // An unknown value is ignored, as in HTML5.
    void setValue(const char * s) {
      if (strcmp(s, "butt") == 0) value = LineCap::BUTT;
      else if (strcmp(s, "round") == 0) value = LineCap::ROUND;
      else if (strcmp(s, "square") == 0) value = LineCap::SQUARE;
    }

    LineCap value;
  };

  class LineJoinAttribute : public AttributeBase {
  public:
    LineJoinAttribute(GraphicsState * _context, LineJoin _value = LineJoin::MITER) : AttributeBase(_context), value(_value) { }
    LineJoinAttribute(const LineJoinAttribute & other) : AttributeBase(other), value(other.value) { }

    LineJoinAttribute & operator=(const LineJoinAttribute & other) { value = other.value; return *this; }
    LineJoinAttribute & operator=(LineJoin other) { value = other; return *this; }
    LineJoinAttribute & operator=(const std::string & s) { setValue(s.c_str()); return *this; }
    LineJoinAttribute & operator=(const char * s) { setValue(s); return *this; }
    Context & operator()(LineJoin other) { value = other; return getContext(); }
    Context & operator()(const std::string & s) { setValue(s.c_str()); return getContext(); }
    Context & operator()(const char * s) { setValue(s); return getContext(); }

    LineJoin get() const { return value; }

  private:
    void setValue(const char * s) {
      if (strcmp(s, "miter") == 0) value = LineJoin::MITER;
      else if (strcmp(s, "round") == 0) value = LineJoin::ROUND;
      else if (strcmp(s, "bevel") == 0) value = LineJoin::BEVEL;
    }

    LineJoin value;
  };
};

#endif
