#ifndef _CANVAS_STYLE_H_
#define _CANVAS_STYLE_H_

#include <Attribute.h>

#include <Color.h>
#include <Filter.h>
#include <Point.h>
#include <TransformationMatrix.h>

#include <string>
#include <map>
#include <memory>

namespace canvas {
  class Style : public AttributeBase {
  public:
    enum StyleType {
      SOLID = 1,
      LINEAR_GRADIENT,
      RADIAL_GRADIENT,
      PATTERN,
      FILTER
    };
    Style(GraphicsState * _context) : AttributeBase(_context) { }
    Style(const Style & other)
      : AttributeBase(other),
	color(other.color),
	p0(other.p0),
	p1(other.p1),
	r0(other.r0),
	r1(other.r1),
	type(other.type),
	colors(other.colors),
	filter(other.filter) { }

    Style & operator=(const std::string & s) {
      color = s;
      type = SOLID;
      return *this;
    }

    Style & operator=(const Color & _color) {
      color = _color;
      type = SOLID;
      return *this;
    }

    Style & operator=(const Style & other) {
      color = other.color;
      p0 = other.p0;
      p1 = other.p1;
      r0 = other.r0;
      r1 = other.r1;
      type = other.type;
      colors = other.colors;
      filter = other.filter;
      return *this;
    }

    Context & operator()(const std::string & s) {
      color = s;
      type = SOLID;
      return getContext();
    }
    
    Context & operator()(const Color & _color) {
      color = _color;
      type = SOLID;
      return getContext();
    }

    StyleType getType() const { return type; }
    void setType(StyleType _type) { type = _type; }

    void addColorStop(float f, const Color & c) {
      colors[f] = c;
    }
    void addColorStop(float f, const std::string & s) {
      colors[f] = s;
    }
    void setVector(double _x0, double _y0, double _x1, double _y1) {
      p0 = Point(_x0, _y0);
      p1 = Point(_x1, _y1);
    }

    // A radial gradient runs from the circle (p0, r0) to the circle (p1, r1).
    void setRadii(double _r0, double _r1) {
      r0 = _r0;
      r1 = _r1;
    }

    const std::map<float, Color> & getColors() const { return colors; }

    Style transform(const TransformationMatrix & matrix) const {
      if (type == LINEAR_GRADIENT || type == RADIAL_GRADIENT) {
	auto s = *this;
	s.p0 = matrix.multiply(s.p0);
	s.p1 = matrix.multiply(s.p1);
	s.r0 *= matrix.transformSize(1.0);
	s.r1 *= matrix.transformSize(1.0);
	return s;
      } else {
	return *this;
      }
    }
    
    Color color;
    Point p0, p1;
    double r0 = 0.0, r1 = 0.0;

  private:
    StyleType type = SOLID;
    std::map<float, Color> colors;
    std::shared_ptr<Filter> filter;
  };
};

#endif
