#ifndef _CANVAS_TEXTALIGN_H_
#define _CANVAS_TEXTALIGN_H_

#include "Attribute.h"

namespace canvas {
  enum class TextAlign {
    START = 1,
    END,
    LEFT,
    CENTER,
    RIGHT
  };

  class TextAlignAttribute : public AttributeBase {
  public:
  TextAlignAttribute(GraphicsState * _context, TextAlign _value = TextAlign::LEFT) : AttributeBase(_context), value(_value) { }
  TextAlignAttribute(GraphicsState * _context, const std::string & _value) : AttributeBase(_context) { setValue(_value.c_str()); }
  TextAlignAttribute(GraphicsState * _context, const char * _value) : AttributeBase(_context) { setValue(_value); }

  TextAlignAttribute(const TextAlignAttribute & other)
    : AttributeBase(other), value(other.value) { }

    TextAlignAttribute & operator=(const TextAlignAttribute & other) { value = other.value; return *this; }
    TextAlignAttribute & operator=(const TextAlign & other) { value = other; return *this; }
    TextAlignAttribute & operator=(const std::string & _value) { setValue(_value.c_str()); return *this; }
    TextAlignAttribute & operator=(const char * _value) { setValue(_value); return *this; }

    TextAlign get() const { return value; }
    
  private:
    void setValue(const char * _value) {
      if (strcmp(_value, "start") == 0) value = TextAlign::START;
      else if (strcmp(_value, "end") == 0) value = TextAlign::END;
      else if (strcmp(_value, "left") == 0) value = TextAlign::LEFT;
      else if (strcmp(_value, "center") == 0) value = TextAlign::CENTER;
      else if (strcmp(_value, "right") == 0) value = TextAlign::RIGHT;
      else {
	value = TextAlign::START;
      }
    };

    TextAlign value;
  };
};

#endif
