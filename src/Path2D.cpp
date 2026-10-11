#include <Path2D.h>

#include <cmath>

using namespace canvas;

void
Path2D::arc(const Point & p, double radius, double sa, double ea, bool anticlockwise) {
  // An arc that starts a path also starts its subpath.
  if (data_.empty()) subpath_start_ = Point(p.x + radius * cos(sa), p.y + radius * sin(sa));
  data_.push_back(PathComponent(PathComponent::ARC, p.x, p.y, radius, sa, ea, anticlockwise));
  current_point_ = Point(p.x + radius * cos(ea), p.y + radius * sin(ea));
}

// Implementation by node-canvas (Node canvas is a Cairo backed Canvas implementation for NodeJS)
// Original implementation influenced by WebKit.
void
Path2D::arcTo(const Point & p1, const Point & p2, double radius) {
  auto p0 = current_point_; // current point may be modified so make a copy
  
  if ((p1.x == p0.x && p1.y == p0.y) || (p1.x == p2.x && p1.y == p2.y) || radius == 0.f) {
    lineTo(p1);
    // p2?
    return;
  }

  Point p1p0((p0.x - p1.x), (p0.y - p1.y));
  Point p1p2((p2.x - p1.x), (p2.y - p1.y));
  float p1p0_length = sqrt(p1p0.x * p1p0.x + p1p0.y * p1p0.y);
  float p1p2_length = sqrt(p1p2.x * p1p2.x + p1p2.y * p1p2.y);

  double cos_phi = (p1p0.x * p1p2.x + p1p0.y * p1p2.y) / (p1p0_length * p1p2_length);
  // all points on a line logic
  if (-1 == cos_phi) {
    lineTo(p1);
    // p2?
    return;
  }

  if (1 == cos_phi) {
    // add infinite far away point
    unsigned int max_length = 65535;
    double factor_max = max_length / p1p0_length;
    Point ep((p0.x + factor_max * p1p0.x), (p0.y + factor_max * p1p0.y));
    lineTo(ep);
    return;
  }

  double tangent = radius / tan(acos(cos_phi) / 2);
  double factor_p1p0 = tangent / p1p0_length;
  Point t_p1p0((p1.x + factor_p1p0 * p1p0.x), (p1.y + factor_p1p0 * p1p0.y));

  Point orth_p1p0(p1p0.y, -p1p0.x);
  double orth_p1p0_length = sqrt(orth_p1p0.x * orth_p1p0.x + orth_p1p0.y * orth_p1p0.y);
  double factor_ra = radius / orth_p1p0_length;

  double cos_alpha = (orth_p1p0.x * p1p2.x + orth_p1p0.y * p1p2.y) / (orth_p1p0_length * p1p2_length);
  if (cos_alpha < 0.f) {
    orth_p1p0 = Point(-orth_p1p0.x, -orth_p1p0.y);
  }

  Point p((t_p1p0.x + factor_ra * orth_p1p0.x), (t_p1p0.y + factor_ra * orth_p1p0.y));

  orth_p1p0 = Point(-orth_p1p0.x, -orth_p1p0.y);
  double sa = acos(orth_p1p0.x / orth_p1p0_length);
  if (orth_p1p0.y < 0.f) {
    sa = 2 * M_PI - sa;
  }

  bool anticlockwise = false;

  double factor_p1p2 = tangent / p1p2_length;
  Point t_p1p2((p1.x + factor_p1p2 * p1p2.x), (p1.y + factor_p1p2 * p1p2.y));
  Point orth_p1p2((t_p1p2.x - p.x),(t_p1p2.y - p.y));
  double orth_p1p2_length = sqrt(orth_p1p2.x * orth_p1p2.x + orth_p1p2.y * orth_p1p2.y);
  double ea = acos(orth_p1p2.x / orth_p1p2_length);

  if (orth_p1p2.y < 0) ea = 2 * M_PI - ea;
  if ((sa > ea) && ((sa - ea) < M_PI)) anticlockwise = true;
  if ((sa < ea) && ((ea - sa) > M_PI)) anticlockwise = true;

  // cerr << "ARC " << int(t_p1p0.x) << " " << int(t_p1p0.y) << " " << int(p.x) << " " << int(p.y) << " " << radius << " " << int(sa * 180.0 / M_PI) << " " << int(ea * 180.0 / M_PI) << " " << (anticlockwise ? "acw" : "cw") << endl;

  lineTo(t_p1p0);
  arc(p, radius, sa, ea, anticlockwise); // && M_PI * 2 != radius);
  // current_point_ = p2;
}

static inline double isLeft(const Point & p0, const Point & p1, const Point & p2) {
  return ((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
}

// The winding number method has been used here. It counts the number
// of times a polygon winds around the point.  If the result is 0, the
// points is outside the polygon.
bool
Path2D::isInside(double x, double y) const {
  Point point(x, y);
  int wn = 0;
  
  for (size_t i = 1; i < data_.size(); i++) {
    size_t i2 = data_[i].type == PathComponent::CLOSE ? 0 : i;
    Point v1( data_[i - 1].x0, data_[i - 1].y0 );
    Point v2( data_[i2].x0, data_[i2].y0 );
    if (v1.y <= point.y) { // start y <= P.y
      if (v2.y > point.y) { // an upward crossing
	if (isLeft(v1, v2, point) > 0) {
	  // point left of edge
	  wn++; // have a valid up intersect
	}
      }
    } else { // start y > P.y (no test needed)
      if (v2.y <= point.y) { // a downward crossing
	if (isLeft(v1, v2, point) < 0) {
	  // point right of edge
	  wn--; // have a valid down intersect
	}
      }
    }
  }
  
  return wn != 0 ? true : false;  
}
