#ifndef _CANVAS_TRANSFORMATIONMATRIX_H_
#define _CANVAS_TRANSFORMATIONMATRIX_H_

#include "Point.h"
#include <cmath>

namespace canvas {
  class TransformationMatrix {
  public:
    TransformationMatrix() : a(1.0), b(0.0), c(0.0), d(1.0), e(0.0), f(0.0) { }
    TransformationMatrix(double _a, double _b, double _c, double _d, double _e, double _f)
      : a(_a), b(_b), c(_c), d(_d), e(_e), f(_f) { }
    
    TransformationMatrix operator* (const TransformationMatrix & other) {
      return multiply(*this, other);    
    }
    
    const TransformationMatrix & operator*= (const TransformationMatrix & other) {
      *this = multiply(*this, other);
      return *this;
    }
    
    Point multiply(double x, double y) const {
      return Point( x * a + y * c + e,
		    x * b + y * d + f
		    );    
    }
    
    Point multiply(const Point & p) const {
      return multiply(p.x, p.y);
    }
    
    double transformAngle(double alpha) const {
      double x = cos(alpha), y = sin(alpha);
      return atan2(x * b + y * d, x * a + y * c);
    }

    // Length scale of the matrix: sqrt(|ad - bc|), so a rotation keeps a
    // size and a mirrored axis doesn't make it negative. For a size that
    // isn't uniform (an image's width and height) see scaleX()/scaleY().
    double transformSize(double s, bool use_sqrt = false) const {
      (void)use_sqrt;
      return s * sqrt(fabs(a * d - b * c));
    }
    double scaleX() const { return hypot(a, b); }
    double scaleY() const { return hypot(c, d); }
    double determinant() const { return a * d - b * c; }

    // Rotation plus uniform scale (plus a mirror if mirrored is set), the
    // transforms under which a circle stays a circle.
    bool isSimilarity(double & rotation, double & scale, bool & mirrored) const {
      scale = hypot(a, b);
      if (scale == 0.0) return false;
      double eps = 1e-9 * scale;
      rotation = atan2(b, a);
      if (fabs(a - d) < eps && fabs(b + c) < eps) { mirrored = false; return true; }
      if (fabs(a + d) < eps && fabs(b - c) < eps) { mirrored = true; return true; }
      return false;
    }

    // Returns false (and leaves result alone) if the matrix is singular.
    bool invert(TransformationMatrix & result) const {
      double det = determinant();
      if (det == 0.0 || !std::isfinite(det)) return false;
      result = TransformationMatrix(d / det, -b / det, -c / det, a / det,
				    (c * f - d * e) / det, (b * e - a * f) / det);
      return true;
    }

  private:
    static TransformationMatrix multiply(const TransformationMatrix & A, const TransformationMatrix & B) {
      return TransformationMatrix( A.a * B.a + A.c * B.b,
				   A.b * B.a + A.d * B.b,
				   A.a * B.c + A.c * B.d,
				   A.b * B.c + A.d * B.d,
				   A.a * B.e + A.c * B.f + A.e,
				   A.b * B.e + A.d * B.f + A.f
				   );
    }
    
    double a, b, c, d, e, f;
  };
};

#endif
