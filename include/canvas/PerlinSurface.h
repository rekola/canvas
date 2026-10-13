#ifndef _CANVAS_PERLINSURFACE_H_
#define _CANVAS_PERLINSURFACE_H_

#include "Surface.h"

namespace canvas {
  // A Perlin-noise image as a source for drawImage(): grey, opaque, 4
  // channels (B, G, R, A premultiplied like the other surfaces' memory).
  // It is not a target, so every drawing call does nothing. The same
  // parameters always give the same pixels.
  class PerlinSurface : public Surface {
  public:
    // octaves layers of noise, each alpha times weaker and beta times finer
    // than the one before. period is the size in pixels of a noise cell in
    // the first layer: noise is zero at whole-number coordinates, so a
    // period of 1 would be flat grey.
    PerlinSurface(float _logical_width, float _logical_height, unsigned int _actual_width, unsigned int _actual_height, int octaves = 1, float alpha = 2, float beta = 2, float period = 32.0f);
    ~PerlinSurface() {
      delete[] buffer;
    }

    void renderPath(RenderMode mode, const TransformationMatrix & transformation, const Path2D & path, const Style & style, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, const LineStyle & lineStyle) override { }
    void renderText(RenderMode mode, const Font & font, const Style & style, TextBaseline textBaseline, TextAlign textAlign, const std::string & text, const Point & p, float lineWidth, Operator op, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath) override { }
    TextMetrics measureText(const Font & font, const std::string & text, TextBaseline textBaseline, float displayScale) override { return TextMetrics(); }
    void drawImage(Surface & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) override { }
    void drawImage(const ImageData & _img, const Point & p, double w, double h, float displayScale, float globalAlpha, float shadowBlur, float shadowOffsetX, float shadowOffsetY, const Color & shadowColor, const Path2D & clipPath, bool imageSmoothingEnabled = true) override { }

    // The noise at actual size.
    std::unique_ptr<Image> createImage(float display_scale) override;

  protected:
    void * lockMemory(bool write_access = false) override;
    void releaseMemory() override {
      delete[] buffer;
      buffer = 0;
    }

    float evaluatePerlin(float x, float y, float z);
    float evaluate(float x, float y, float z);

  private:
    unsigned char * buffer = 0;

    int octaves;
    float alpha, beta, period;
    int p[512];
  };
};

#endif /* _PERLINSURFACE_H_ */
