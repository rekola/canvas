#ifndef _CANVAS_TEXTBASELINE_H_
#define _CANVAS_TEXTBASELINE_H_

#include "Attribute.h"

#include <cstring>

namespace canvas {
  enum class TextBaseline {
    TOP = 1,
    HANGING,
    MIDDLE,
    ALPHABETIC,
    IDEOGRAPHIC,
    BOTTOM
  };
  
  // How far below the requested y the alphabetic baseline is drawn, given
  // the font's ascent and descent (positive numbers, any unit). Hanging is
  // approximated as 0.8 of the ascent, ideographic as the bottom of the
  // font box.
  inline float getBaselineOffset(TextBaseline baseline, float ascent, float descent) {
    switch (baseline) {
    case TextBaseline::TOP: return ascent;
    case TextBaseline::HANGING: return 0.8f * ascent;
    case TextBaseline::MIDDLE: return (ascent - descent) / 2;
    case TextBaseline::IDEOGRAPHIC:
    case TextBaseline::BOTTOM: return -descent;
    default: return 0;
    }
  }

  class TextBaselineAttribute : public AttributeBase {
  public:
    TextBaselineAttribute(GraphicsState * _context, TextBaseline _value = TextBaseline::ALPHABETIC) : AttributeBase(_context), value(_value) { }
  TextBaselineAttribute(GraphicsState * _context, const std::string & _value) : AttributeBase(_context) { setValue(_value.c_str()); }
  TextBaselineAttribute(GraphicsState * _context, const char * _value) : AttributeBase(_context) { setValue(_value); }

  TextBaselineAttribute(const TextBaselineAttribute & other)
    : AttributeBase(other), value(other.value) { }

    TextBaselineAttribute & operator=(const TextBaselineAttribute & other) { value = other.value; return *this; }
    TextBaselineAttribute & operator=(const TextBaseline & _value) { value = _value; return *this; }
    TextBaselineAttribute & operator=(const std::string & _value) { setValue(_value.c_str()); return *this; }
    TextBaselineAttribute & operator=(const char * _value) { setValue(_value); return *this; }

    TextBaseline get() const { return value; }

  private:
    void setValue(const char * _value) {
      if (strcmp(_value, "top") == 0) value = TextBaseline::TOP;
      else if (strcmp(_value, "hanging") == 0) value = TextBaseline::HANGING;
      else if (strcmp(_value, "middle") == 0) value = TextBaseline::MIDDLE;
      else if (strcmp(_value, "alphabetic") == 0) value = TextBaseline::ALPHABETIC;
      else if (strcmp(_value, "ideographic") == 0) value = TextBaseline::IDEOGRAPHIC;
      else if (strcmp(_value, "bottom") == 0) value = TextBaseline::BOTTOM;
      else {
	value = TextBaseline::TOP;
      }
    };

    TextBaseline value;
  };
};

#endif
