#ifndef _CANVAS_CONTEXT_H_
#define _CANVAS_CONTEXT_H_

#include "GraphicsState.h"

#include "Color.h"
#include "Surface.h"
#include "Image.h"
#include "HitRegion.h"

#include <cmath>
#include <string>
#include <memory>

namespace canvas {
  class Context : public GraphicsState {
  public:
    Context(float display_scale = 1.0f)
      : display_scale_(display_scale)
      { }
    Context(const Context & other) = delete;
    Context & operator=(const Context & other) = delete;
    Context & operator=(const GraphicsState & other) {
      GraphicsState::operator=(other);
      return *this;
    }
    virtual ~Context() = default;

    virtual std::unique_ptr<Surface> createSurface(const ImageData & image) = 0;
    virtual std::unique_ptr<Surface> createSurface(unsigned int _width, unsigned int _height, unsigned int _num_channels) = 0;
    // A surface of exactly this many pixels; its logical size is
    // size / getDisplayScale() and may be fractional.
    virtual std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int _num_channels) = 0;
    virtual Surface & getDefaultSurface() = 0;
    virtual const Surface & getDefaultSurface() const = 0;

    virtual bool hasNativeShadows() const { return false; }
    virtual bool hasNativeEmoticons() const { return false; }

    virtual void resize(unsigned int _width, unsigned int _height) {
      getDefaultSurface().resize(_width, _height, toPixels(_width, getDisplayScale()), toPixels(_height, getDisplayScale()), getDefaultSurface().getNumChannels());
      hit_regions_.clear();
    }
    // Resizes to exactly this many pixels (see createSurface(PixelSize)).
    virtual void resize(PixelSize size) {
      getDefaultSurface().resize(size.width / getDisplayScale(), size.height / getDisplayScale(), size.width, size.height, getDefaultSurface().getNumChannels());
      hit_regions_.clear();
    }
        
    // The current path is in device coordinates already; a Path2D passed
    // in is in user coordinates and gets the current transform.
    Context & stroke() { return renderPath(RenderMode::STROKE, currentPath, strokeStyle); }
    Context & stroke(const Path2D & path) { return renderPath(RenderMode::STROKE, path.transform(currentTransform), strokeStyle); }
    Context & fill() { return renderPath(RenderMode::FILL, currentPath, fillStyle); }
    Context & fill(const Path2D & path) { return renderPath(RenderMode::FILL, path.transform(currentTransform), fillStyle); }
    Context & save() {
      restore_stack_.push_back(*this);
      return *this;
    }
    // The current path is not part of the drawing state (HTML5), so it
    // survives restore().
    Context & restore() {
      if (!restore_stack_.empty()) {
	auto path = std::move(currentPath);
	*this = restore_stack_.back();
	currentPath = std::move(path);
	restore_stack_.pop_back();    
      }
      return *this;
    }

    // GraphicsState's, returning Context& so calls chain.
    Context & clip() { GraphicsState::clip(); return *this; }
    Context & resetClip() { GraphicsState::resetClip(); return *this; }
    Context & scale(double x, double y) { GraphicsState::scale(x, y); return *this; }
    Context & rotate(double angle) { GraphicsState::rotate(angle); return *this; }
    Context & translate(double x, double y) { GraphicsState::translate(x, y); return *this; }
    Context & transform(double a, double b, double c, double d, double e, double f) { GraphicsState::transform(a, b, c, d, e, f); return *this; }
    Context & transform(const TransformationMatrix & m) { GraphicsState::transform(m); return *this; }
    Context & setTransform(double a, double b, double c, double d, double e, double f) { GraphicsState::setTransform(a, b, c, d, e, f); return *this; }
    Context & setTransform(const TransformationMatrix & m) { GraphicsState::setTransform(m); return *this; }
    Context & resetTransform() { GraphicsState::resetTransform(); return *this; }
    
    // x and y are device coordinates, the path is in user coordinates.
    bool isPointInPath(const Path2D & path, double x, double y) { return path.transform(currentTransform).isInside(x, y); }
    bool isPointInPath(double x, double y) { return currentPath.isInside(x, y); }
    
    TextMetrics measureText(const std::string & text) {
      return getDefaultSurface().measureText(font, text, textBaseline.get(), getDisplayScale());
    }

    // Points are transformed when they are added (HTML5).
    Context & moveTo(double x, double y) { currentPath.moveTo(currentTransform.multiply(x, y)); return *this; }
    Context & lineTo(double x, double y) { currentPath.lineTo(currentTransform.multiply(x, y)); return *this; }
    Context & arcTo(double x1, double y1, double x2, double y2, double radius) {
      TransformationMatrix inverse;
      if (currentPath.empty()) {
	moveTo(x1, y1);
      } else if (currentTransform.invert(inverse)) {
	// The corner is worked out in user space from the current point.
	Path2D path;
	path.moveTo(inverse.multiply(currentPath.getCurrentPoint()));
	path.arcTo(Point(x1, y1), Point(x2, y2), radius);
	currentPath.append(path.transform(currentTransform), 1);
      }
      return *this;
    }

    Context & quadraticCurveTo(double cpx, double cpy, double x, double y) {
      return addCurve(false, cpx, cpy, 0, 0, x, y);
    }
    Context & bezierCurveTo(double cp1x, double cp1y, double cp2x, double cp2y, double x, double y) {
      return addCurve(true, cp1x, cp1y, cp2x, cp2y, x, y);
    }

    Context & arc(double x, double y, double r, double a0, double a1, bool t = false) {
      Path2D path;
      path.arc(Point(x, y), r, a0, a1, t);
      currentPath.append(path.transform(currentTransform), 0, true);
      return *this;
    }

    Context & fillRect(double x, double y, double w, double h) {
      Path2D path;
      path.rect(x, y, w, h);
      return fill(path);
    }
    
    Context & strokeRect(double x, double y, double w, double h) {
      Path2D path;
      path.rect(x, y, w, h);
      return stroke(path);
    }
    
    Context & clearRect(double x, double y, double w, double h) {
      Path2D path;
      path.rect(x, y, w, h);
      Style style(this);
      style = Color(0.0f, 0.0f, 0.0f, 0.0f);
      return renderPath(RenderMode::FILL, path.transform(currentTransform), style, Operator::COPY);
    }

    Context & rect(double x, double y, double w, double h) {
      Path2D path;
      path.rect(x, y, w, h);
      currentPath.append(path.transform(currentTransform));
      return *this;
    }

    Context & beginPath() { currentPath.clear(); return *this; }
    Context & closePath() { currentPath.closePath(); return *this; }

    Context & fillText(const std::string & text, double x, double y) { return renderText(RenderMode::FILL, fillStyle, text, Point(x, y)); }
    Context & strokeText(const std::string & text, double x, double y) { return renderText(RenderMode::STROKE, strokeStyle, text, Point(x, y)); }

    // As in HTML5: an odd number of lengths is repeated to make an even
    // number, and a negative or non-finite length ignores the call.
    Context & setLineDash(std::vector<float> dash) {
      for (auto v : dash) {
	if (!(v >= 0.0f) || !std::isfinite(v)) return *this;
      }
      if (dash.size() % 2 == 1) {
	auto copy = dash;
	dash.insert(dash.end(), copy.begin(), copy.end());
      }
      lineDash = dash;
      return *this;
    }
    const std::vector<float> & getLineDash() const { return lineDash; }
    
    // Logical size; fractional for a pixel size at a fractional scale.
    float getWidth() const { return getDefaultSurface().getLogicalWidth(); }
    float getHeight() const { return getDefaultSurface().getLogicalHeight(); }
    unsigned int getActualWidth() const { return getDefaultSurface().getActualWidth(); }
    unsigned int getActualHeight() const { return getDefaultSurface().getActualHeight(); }

    Context & drawImage(Context & other, double x, double y, double w, double h) {
      return drawImage(other.getDefaultSurface(), x, y, w, h);
    }
    
    Context & drawImage(Image & img, double x, double y, double w, double h) {
      return drawImage(img.getData(), x, y, w, h);
    }
    
    virtual Context & drawImage(const ImageData & img, double x, double y, double w0, double h0) {
      auto p = currentTransform.multiply(x, y);
      auto w = w0 * currentTransform.scaleX();
      auto h = h0 * currentTransform.scaleY();
      if (hasNativeShadows()) {
	getDefaultSurface().drawImage(img, p, w, h, getDisplayScale(), globalAlpha.get(), shadowBlur.get(), shadowOffsetX.get(), shadowOffsetY.get(), shadowColor.get(), clipPath, imageSmoothingEnabled.get());
      } else {
	if (hasShadow()) {
	  float b = shadowBlur.get(), bs = shadowBlur.get() * getDisplayScale();
	  float bi = int(ceil(b));
	  auto shadow = createShadowSurface(bi, 1);
	  shadow->drawImage(img, Point(x + b + shadowOffsetX.get(), y + b + shadowOffsetY.get()), w, h, getDisplayScale(), globalAlpha.get(), 0.0f, 0.0f, 0.0f, shadowColor.get(), clipPath, imageSmoothingEnabled.get());
	  // shadow->colorFill(shadowColor.get());
	  auto shadow1 = shadow->blur(bs, bs);
	  auto shadow2 = shadow1->colorize(shadowColor.get());
	  getDefaultSurface().drawImage(*shadow2, Point(-b, -b), shadow->getLogicalWidth(), shadow->getLogicalHeight(), getDisplayScale(), 1.0f, 0.0f, 0.0f, 0.0f, shadowColor.get(), Path2D(), false);
	}
	getDefaultSurface().drawImage(img, p, w, h, getDisplayScale(), globalAlpha.get(), 0.0f, 0.0f, 0.0f, shadowColor.get(), clipPath, imageSmoothingEnabled.get());
      }
      return *this;
    }
    
    virtual Context & drawImage(Surface & img, double x, double y, double w0, double h0) {
      auto p = currentTransform.multiply(x, y);
      auto w = w0 * currentTransform.scaleX();
      auto h = h0 * currentTransform.scaleY();
      if (hasNativeShadows()) {
	getDefaultSurface().drawImage(img, p, w, h, getDisplayScale(), globalAlpha.get(), shadowBlur.get(), shadowOffsetX.get(), shadowOffsetY.get(), shadowColor.get(), clipPath, imageSmoothingEnabled.get());
      } else {
	if (hasShadow()) {
	  float b = shadowBlur.get(), bs = shadowBlur.get() * getDisplayScale();
	  float bi = int(ceil(b));
	  auto shadow = createShadowSurface(bi, 1);
	  
	  shadow->drawImage(img, Point(p.x + b + shadowOffsetX.get(), p.y + b + shadowOffsetY.get()), w, h, getDisplayScale(), globalAlpha.get(), 0.0f, 0.0f, 0.0f, shadowColor.get(), clipPath, imageSmoothingEnabled.get());
	  // shadow->colorFill(shadowColor.get());
	  auto shadow1 = shadow->blur(bs, bs);
	  auto shadow2 = shadow1->colorize(shadowColor.get());
	  getDefaultSurface().drawImage(*shadow2, Point(-b, -b), shadow->getLogicalWidth(), shadow->getLogicalHeight(), getDisplayScale(), 1.0f, 0.0f, 0.0f, 0.0f, shadowColor.get(), Path2D(), false);
	}
	getDefaultSurface().drawImage(img, p, w, h, getDisplayScale(), globalAlpha.get(), 0.0f, 0.0f, 0.0f, shadowColor.get(), clipPath, imageSmoothingEnabled.get());
      }
      return *this;
    }
        
    // A new gradient each call. Unlike HTML5's CanvasGradient it is a
    // value: add its stops before assigning it to fillStyle/strokeStyle.
    Style createLinearGradient(double x0, double y0, double x1, double y1) {
      Style gradient(this);
      gradient.setType(Style::LINEAR_GRADIENT);
      gradient.setVector(x0, y0, x1, y1);
      return gradient;
    }

    // The gradient runs from the circle (x0, y0, r0) to the circle (x1, y1, r1)
    // and is a value like the linear one. It pads beyond both circles.
    Style createRadialGradient(double x0, double y0, double r0, double x1, double y1, double r1) {
      Style gradient(this);
      gradient.setType(Style::RADIAL_GRADIENT);
      gradient.setVector(x0, y0, x1, y1);
      gradient.setRadii(r0, r1);
      return gradient;
    }

    float getDisplayScale() const { return display_scale_; }
    Context & addHitRegion(std::string id, std::string cursor = "") {
      if (!currentPath.empty()) {
	auto path = currentPath.flatten();
	hit_regions_.push_back(HitRegion(std::move(id), std::move(path), std::move(cursor)));
      }
      return *this;
    }
    Context & addHitRegion(HitRegion hit_region) {
      if (!currentPath.empty()) {
	auto path = currentPath.flatten();
	hit_region.setPath(std::move(path));
	hit_regions_.push_back(std::move(hit_region));
      }
      return *this;
    }
#if 0
    const HitRegion & getHitRegion(float x, float y) const {
      for (auto & r : hit_regions_) {
	if (r.isInside(x, y)) return r;
      }
      return null_region;
    }
#endif
    const std::vector<HitRegion> & getHitRegions() const { return hit_regions_; }
    
#if 0
    Style & createPattern(const ImageData & image, const char * repeat) {
      
    }
#endif
    
  protected:
    // The default surface grown by at least pad logical units on each
    // side, in whole pixels so that a fractional logical size stays exact.
    std::unique_ptr<Surface> createShadowSurface(float pad, unsigned int num_channels) {
      unsigned int p = (unsigned int)ceil(pad * getDisplayScale() - 1e-4);
      return createSurface(PixelSize{ getDefaultSurface().getActualWidth() + 2 * p, getDefaultSurface().getActualHeight() + 2 * p }, num_channels);
    }

    // The stroke width after the transform. A lineWidth of 0 is a hairline:
    // one device pixel whatever the transform and display scale (the
    // backends multiply by the scale, so it is passed as 1 / scale).
    float getScaledLineWidth() const {
      if (lineWidth.get() == 0.0f) return 1.0f / getDisplayScale();
      return currentTransform.transformSize(lineWidth.get(), true);
    }

    LineStyle getLineStyle() const {
      LineStyle line;
      line.cap = lineCap.get();
      line.join = lineJoin.get();
      line.miterLimit = miterLimit.get();
      line.dashOffset = lineDashOffset.get();
      line.dash = lineDash;
      return line;
    }

    // The curve is worked out in user space from the current point, then
    // transformed with the rest of the path.
    Context & addCurve(bool cubic, double c1x, double c1y, double c2x, double c2y, double x, double y) {
      TransformationMatrix inverse;
      Path2D path;
      double scale = currentTransform.transformSize(1.0);
      size_t first = 0;
      if (!currentPath.empty()) {
	if (!currentTransform.invert(inverse)) return *this;
	path.moveTo(inverse.multiply(currentPath.getCurrentPoint()));
	first = 1;
      }
      if (cubic) path.bezierCurveTo(c1x, c1y, c2x, c2y, x, y, scale);
      else path.quadraticCurveTo(c1x, c1y, x, y, scale);
      currentPath.append(path.transform(currentTransform), first);
      return *this;
    }

    // The path is in device coordinates.
    Context & renderPath(RenderMode mode, const Path2D & path, const Style & style0, Operator op = Operator::SOURCE_OVER) {
      auto scaled_lineWidth = getScaledLineWidth();
      if (mode != RenderMode::STROKE || lineWidth.get() == 0.0f || scaled_lineWidth > 0.1f) {
	auto style = style0.transform(currentTransform);
	
	if (hasNativeShadows()) {
	  getDefaultSurface().renderPath(mode, currentTransform, path, style, scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), shadowBlur.get(), shadowOffsetX.get(), shadowOffsetY.get(), shadowColor.get(), clipPath, getLineStyle());
	} else {
	  if (hasShadow()) {
	    float b = shadowBlur.get(), bs = shadowBlur.get() * getDisplayScale();
	    float bi = int(ceil(b));
#ifdef WIN32
	    auto shadow = createShadowSurface(bi, 4);
	    Style shadow_style(this);
	    shadow_style = shadowColor.get();
	    Path2D tmp_path = path, tmp_clipPath = clipPath;
	    tmp_path.offset(shadowOffsetX.get() + bi, shadowOffsetY.get() + bi);
	    tmp_clipPath.offset(shadowOffsetX.get() + bi, shadowOffsetY.get() + bi);
	    
	    shadow->renderPath(mode, currentTransform, tmp_path, shadow_style, scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), 0, 0, 0, shadowColor.get(), tmp_clipPath, getLineStyle());
	    auto shadow1 = shadow->blur(bs, bs);
	    getDefaultSurface().drawImage(*shadow1, Point(-b, -b), shadow->getLogicalWidth(), shadow->getLogicalHeight(), getDisplayScale(), 1.0f, 0.0f, 0.0f, 0.0f, shadowColor.get(), Path2D(), false);
#else
	    auto shadow = createShadowSurface(bi, 1);
	    Style shadow_style(this);
	    shadow_style = shadowColor.get();
	    Path2D tmp_path = path, tmp_clipPath = clipPath;
	    tmp_path.offset(shadowOffsetX.get() + bi, shadowOffsetY.get() + bi);
	    tmp_clipPath.offset(shadowOffsetX.get() + bi, shadowOffsetY.get() + bi);
	    
	    shadow->renderPath(mode, currentTransform, tmp_path, shadow_style, scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), 0, 0, 0, shadowColor.get(), tmp_clipPath, getLineStyle());
	    auto shadow1 = shadow->blur(bs, bs);
	    auto shadow2 = shadow1->colorize(shadowColor.get());
	    getDefaultSurface().drawImage(*shadow2, Point(-b, -b), shadow->getLogicalWidth(), shadow->getLogicalHeight(), getDisplayScale(), 1.0f, 0.0f, 0.0f, 0.0f, shadowColor.get(), Path2D(), false);
#endif
	  }
	  getDefaultSurface().renderPath(mode, currentTransform, path, style, scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), 0, 0, 0, shadowColor.get(), clipPath, getLineStyle());
	}
      }
      return *this;
    }
    
    Context & renderText(RenderMode mode, const Style & style0, const std::string & text, const Point & p0, Operator op = Operator::SOURCE_OVER) {
      auto scaled_font_size = currentTransform.transformSize(font.size);
      auto scaled_lineWidth = getScaledLineWidth();
      if (scaled_font_size > 0.5f && (mode != RenderMode::STROKE || lineWidth.get() == 0.0f || scaled_lineWidth > 0.1f)) {
	auto p = currentTransform.multiply(p0);
	auto tmp_font = font;
	tmp_font.size = scaled_font_size;

	auto style = style0.transform(currentTransform);

	if (hasNativeShadows()) {
	  getDefaultSurface().renderText(mode, tmp_font, style, textBaseline.get(), textAlign.get(), text, p, scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), shadowBlur.get(), shadowOffsetX.get(), shadowOffsetY.get(), shadowColor.get(), clipPath);
	} else {
	  if (hasShadow()) {
	    float b = shadowBlur.get(), bs = shadowBlur.get() * getDisplayScale();
	    int bi = int(ceil(b));
	    auto shadow = createShadowSurface(bi, 1);
	  
	    Style shadow_style(this);
	    shadow_style = shadowColor.get();
	    shadow_style.color.alpha = 1.0f;
	    shadow->renderText(mode, tmp_font, shadow_style, textBaseline.get(), textAlign.get(), text, Point(p.x + shadowOffsetX.get() + b, p.y + shadowOffsetY.get() + b), scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), 0.0f, 0.0f, 0.0f, shadowColor.get(), clipPath);
	    auto shadow1 = shadow->blur(bs, bs);
	    auto shadow2 = shadow1->colorize(shadowColor.get());
	    getDefaultSurface().drawImage(*shadow2, Point(-b, -b), shadow->getLogicalWidth(), shadow->getLogicalHeight(), getDisplayScale(), 1.0f, 0.0f, 0.0f, 0.0f, shadowColor.get(), Path2D(), false);
	  }
	  getDefaultSurface().renderText(mode, tmp_font, style, textBaseline.get(), textAlign.get(), text, p, scaled_lineWidth, op, getDisplayScale(), globalAlpha.get(), 0.0f, 0.0f, 0.0f, shadowColor.get(), clipPath);
	}
      }
      return *this;
    }

    bool hasShadow() const { return shadowBlur.get() > 0.0f || shadowOffsetX.get() != 0 || shadowOffsetY.get() != 0; }
    
  private:
    float display_scale_;
    std::vector<GraphicsState> restore_stack_;
    std::vector<HitRegion> hit_regions_;

    static inline HitRegion null_region;
  };
    
  inline Context & AttributeBase::getContext() { return static_cast<Context &>(*context); }

  class ContextFactory {
  public:
    ContextFactory(float display_scale) : display_scale_(display_scale) { }
    virtual ~ContextFactory() { }
    virtual std::unique_ptr<Context> createContext(unsigned int width, unsigned int height, unsigned int num_channels = 4) = 0;
    virtual std::unique_ptr<Surface> createSurface(unsigned int width, unsigned int height, unsigned int num_channels = 4) = 0;
    // Exact pixel sizes; the logical size is size / getDisplayScale().
    virtual std::unique_ptr<Context> createContext(PixelSize size, unsigned int num_channels = 4) = 0;
    virtual std::unique_ptr<Surface> createSurface(PixelSize size, unsigned int num_channels = 4) = 0;
    virtual std::unique_ptr<Image> loadImage(const std::string & filename) = 0;
    virtual std::unique_ptr<Image> createImage() = 0;
    virtual std::unique_ptr<Image> createImage(const unsigned char * _data, unsigned int _width, unsigned int _height, unsigned int _num_channels) = 0;
    
    float getDisplayScale() const { return display_scale_; }
    
  private:
    float display_scale_;
  };

  class NullContext : public Context {
  public:
    NullContext() { }
  };
};

#endif
