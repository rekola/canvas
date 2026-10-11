#ifndef _CANVAS_ATTRIBUTE_H_
#define _CANVAS_ATTRIBUTE_H_

#include <string>
#include <stdlib.h>

namespace canvas {
  class GraphicsState;
  class Context;

  inline void assignValue(float & a, const std::string & b) {
    a = std::stof(b);
  }
  inline void assignValue(bool & a, const std::string & b) {
    a = !b.empty() && b != "0" && b != "false";
  }

  class AttributeBase {
  public:
    AttributeBase(GraphicsState * _context) : context(_context) { }
    AttributeBase(const AttributeBase & other) : context(other.context) { }
    // Assigning copies the value only: an attribute keeps its owner.
    AttributeBase & operator=(const AttributeBase &) { return *this; }

  protected:
    // The owner, for the fluent setters; defined in Context.h.
    inline Context & getContext();
    
  private:
    GraphicsState * context;
  };

  template<class T>
  class Attribute : public AttributeBase {
  public:
    Attribute(GraphicsState * _context) : AttributeBase(_context), value() { }
    Attribute(GraphicsState * _context, T _value) : AttributeBase(_context), value(_value) { }
    Attribute(const Attribute<T> & other) : AttributeBase(other), value(other.value) { }
  
    Attribute<T> & operator=(const Attribute<T> & other) {
      value = other.value;
      return *this;
    }
    Attribute<T> & operator=(T _value) {
      value = _value;
      return *this;
    }
    Attribute<T> & operator=(const std::string & s) {
      assignValue(value, s);
      return *this;
    }
    Context & operator()(T _value) {
      value = _value;
      return getContext();
    }
    Context & operator()(const std::string & s) {
      assignValue(value, s);
      return getContext();
    }

    T get() const { return value; }
    
  private:
    T value;
  };
};

#endif
