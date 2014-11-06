#include "Context.h"

#include <cmath>
#include <iostream>

#ifdef OPENGL
#include <glm/glm.hpp>
#endif

using namespace std;
using namespace canvas;

void
Context::resize(unsigned int _width, unsigned int _height) {
  width = _width;
  height = _height;
  getDefaultSurface().resize(_width, _height);
}

void
Context::rect(double x, double y, double w, double h) {
  moveTo(x, y);
  lineTo(x + w, y);
  lineTo(x + w, y + h);
  lineTo(x, y + h); 
  closePath();
}

void
Context::fillRect(double x, double y, double w, double h) {
  save();
  beginPath();
  rect(x, y, w, h);  
  fill();
  beginPath(); // tmp fix
  restore();
} 

void
Context::strokeRect(double x, double y, double w, double h) {
  beginPath();
  rect(x, y, w, h);  
  stroke();
}

void
Context::fillText(const std::string & text, double x, double y) {  
  if (hasShadow()) {
    float bx = shadowBlurX ? shadowBlurX : shadowBlur, by = shadowBlurY ? shadowBlurY : shadowBlur;
    auto shadow = createSurface(getDefaultSurface().getWidth() + 2 * bx, getDefaultSurface().getHeight() + 2 * by);
    Style tmp_style = fillStyle;
    tmp_style.color = shadowColor;
    shadow->fillText(font, tmp_style, textBaseline, textAlign, text, x + shadowOffsetX + bx, y + shadowOffsetY + by);
    shadow->gaussianBlur(bx, by);
    getDefaultSurface().drawImage(*shadow, -bx, -by, shadow->getWidth(), shadow->getHeight());
  }
  getDefaultSurface().fillText(font, fillStyle, textBaseline, textAlign, text, x, y);
}

void
Context::strokeText(const std::string & text, double x, double y) {  
  if (hasShadow()) {
    float bx = shadowBlurX ? shadowBlurX : shadowBlur, by = shadowBlurY ? shadowBlurY : shadowBlur;
    auto shadow = createSurface(getDefaultSurface().getWidth() + 2 * bx, getDefaultSurface().getHeight() + 2 * by);
    Style tmp_style = strokeStyle;
    tmp_style.color = shadowColor;
    shadow->strokeText(font, tmp_style, textBaseline, textAlign, text, x + shadowOffsetX + bx, y + shadowOffsetY + by);
    shadow->gaussianBlur(bx, by);
    getDefaultSurface().drawImage(*shadow, -bx, -by, shadow->getWidth(), shadow->getHeight());
  }
  getDefaultSurface().strokeText(font, strokeStyle, textBaseline, textAlign, text, x, y);
}

void
Context::fill() {
  if (hasShadow()) {
    float bx = shadowBlurX ? shadowBlurX : shadowBlur, by = shadowBlurY ? shadowBlurY : shadowBlur;
    auto shadow = createSurface(getDefaultSurface().getWidth() + 2 * bx, getDefaultSurface().getHeight() + 2 * by);
    Style tmp_style = fillStyle;
    tmp_style.color = shadowColor;
    Path tmp_path = current_path;
    tmp_path.offset(shadowOffsetX + bx, shadowOffsetY + by);
    
    shadow->fill(tmp_path, tmp_style);
    shadow->gaussianBlur(bx, by);
    
    getDefaultSurface().drawImage(*shadow, -bx, -by, shadow->getWidth(), shadow->getHeight());
  }
  getDefaultSurface().fill(current_path, fillStyle);
}

void
Context::stroke() {
  if (hasShadow()) {
    float bx = shadowBlurX ? shadowBlurX : shadowBlur, by = shadowBlurY ? shadowBlurY : shadowBlur;
    auto shadow = createSurface(getDefaultSurface().getWidth() + 2 * bx, getDefaultSurface().getHeight() + 2 * by);
    Style tmp_style = strokeStyle;
    tmp_style.color = shadowColor;
    Path tmp_path = current_path;
    tmp_path.offset(shadowOffsetX + bx, shadowOffsetY + by);
    
    shadow->stroke(tmp_path, tmp_style, lineWidth);
    shadow->gaussianBlur(bx, by);
    
    getDefaultSurface().drawImage(*shadow, -bx, -by, shadow->getWidth(), shadow->getHeight());    
  }
  getDefaultSurface().stroke(current_path, strokeStyle, lineWidth);
}

void
Context::drawImage(Surface & img, double x, double y, double w, double h) {
  if (hasShadow()) {
    float bx = shadowBlurX ? shadowBlurX : shadowBlur, by = shadowBlurY ? shadowBlurY : shadowBlur;
    auto shadow = createSurface(getDefaultSurface().getWidth() + 2 * bx, getDefaultSurface().getHeight() + 2 * by);
    
    shadow->drawImage(img, x + shadowOffsetX + bx, y + shadowOffsetY + by, w, h);
    shadow->colorFill(shadowColor);
    shadow->gaussianBlur(bx, by);
    
    getDefaultSurface().drawImage(*shadow, -bx, -by, shadow->getWidth(), shadow->getHeight());    
  }
  getDefaultSurface().drawImage(img, x, y, w, h);
}
