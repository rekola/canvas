#include <Path2D.h>

#include <algorithm>
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

Path2D
Path2D::transform(const TransformationMatrix & matrix) const {
  double rotation = 0.0, scale = 1.0;
  bool mirrored = false, similar = matrix.isSimilarity(rotation, scale, mirrored);
  Path2D new_path;
  for (auto pc : data_) {
    if (pc.type == PathComponent::ARC && !similar) {
      Path2D arc;
      arc.data_.push_back(pc);
      new_path.append(arc.flatten().transform(matrix), 0, true);
      continue;
    }
    auto p = matrix.multiply(pc.x0, pc.y0);
    pc.x0 = p.x;
    pc.y0 = p.y;
    if (pc.type == PathComponent::ARC) {
      pc.radius *= scale;
      if (mirrored) {
	// The direction at angle t maps to rotation - t.
	pc.sa = rotation - pc.sa;
	pc.ea = rotation - pc.ea;
	pc.anticlockwise = !pc.anticlockwise;
      } else {
	pc.sa += rotation;
	pc.ea += rotation;
      }
    }
    new_path.data_.push_back(std::move(pc));
  }
  new_path.current_point_ = matrix.multiply(current_point_);
  new_path.subpath_start_ = matrix.multiply(subpath_start_);
  return new_path;
}

Path2D
Path2D::flatten() const {
  Path2D new_path;
  for (auto pc : data_) {
    if (pc.type == PathComponent::ARC) {
      double ea = pc.ea, step;
      if (pc.anticlockwise) {
	while (ea > pc.sa) ea -= 2*M_PI;
      } else {
	while (ea < pc.sa) ea += 2*M_PI;
      }
      step = (ea - pc.sa) / 64.0;
      for (int i = 0; i <= 64; i++) {
	double a = i == 64 ? ea : pc.sa + i * step;
	auto x = pc.x0 + cos(a) * pc.radius;
	auto y = pc.y0 + sin(a) * pc.radius;
	if (new_path.data_.empty()) new_path.moveTo(x, y);
	else new_path.lineTo(x, y);
      }
    } else {
      new_path.data_.push_back(std::move(pc));
    }
  }
  new_path.current_point_ = current_point_;
  new_path.subpath_start_ = subpath_start_;
  return new_path;
}

void
Path2D::append(const Path2D & other, size_t first, bool join) {
  for (size_t i = first; i < other.data_.size(); i++) {
    auto & pc = other.data_[i];
    switch (pc.type) {
    case PathComponent::MOVE_TO:
      if (join && i == first && !data_.empty()) lineTo(pc.x0, pc.y0);
      else moveTo(pc.x0, pc.y0);
      break;
    case PathComponent::LINE_TO: lineTo(pc.x0, pc.y0); break;
    case PathComponent::ARC: arc(Point(pc.x0, pc.y0), pc.radius, pc.sa, pc.ea, pc.anticlockwise); break;
    case PathComponent::CLOSE: closePath(); break;
    }
  }
}

typedef std::vector<Point> Polygon;

// The subpaths of a flattened path as polygons (implicitly closed). A
// segment after a close continues from the closed subpath's start.
static std::vector<Polygon> toPolygons(const Path2D & path) {
  std::vector<Polygon> polygons;
  Polygon current;
  Point start;
  bool closed = false;
  auto flush = [&]() {
    if (current.size() > 1) polygons.push_back(std::move(current));
    current.clear();
  };
  Path2D flat = path.flatten();
  for (auto & pc : flat.getData()) {
    switch (pc.type) {
    case PathComponent::MOVE_TO:
      flush();
      current.push_back(start = Point(pc.x0, pc.y0));
      closed = false;
      break;
    case PathComponent::LINE_TO:
      if (closed) {
	flush();
	current.push_back(start);
	closed = false;
      }
      current.push_back(Point(pc.x0, pc.y0));
      break;
    case PathComponent::CLOSE:
      closed = true;
      break;
    default:
      break;
    }
  }
  flush();
  return polygons;
}

static inline double cross(const Point & a, const Point & b, const Point & c) {
  return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
}

static double signedArea(const Polygon & poly) {
  double sum = 0;
  for (size_t i = 0; i < poly.size(); i++) {
    auto & p = poly[i];
    auto & q = poly[(i + 1) % poly.size()];
    sum += p.x * q.y - q.x * p.y;
  }
  return sum / 2;
}

static bool isConvex(const Polygon & poly) {
  if (poly.size() < 3) return false;
  int sign = 0;
  for (size_t i = 0; i < poly.size(); i++) {
    double c = cross(poly[i], poly[(i + 1) % poly.size()], poly[(i + 2) % poly.size()]);
    if (c == 0) continue;
    int s = c > 0 ? 1 : -1;
    if (sign && s != sign) return false;
    sign = s;
  }
  return sign != 0;
}

// Sutherland-Hodgman: subject clipped by a convex polygon.
static Polygon clipConvex(Polygon subject, Polygon clipper) {
  if (signedArea(clipper) < 0) std::reverse(clipper.begin(), clipper.end());
  for (size_t i = 0; i < clipper.size() && !subject.empty(); i++) {
    auto & a = clipper[i];
    auto & b = clipper[(i + 1) % clipper.size()];
    Polygon out;
    for (size_t j = 0; j < subject.size(); j++) {
      auto & p = subject[j];
      auto & q = subject[(j + 1) % subject.size()];
      double dp = cross(a, b, p), dq = cross(a, b, q);
      if (dp >= 0) out.push_back(p);
      if ((dp > 0 && dq < 0) || (dp < 0 && dq > 0)) {
	double t = dp / (dp - dq);
	out.push_back(Point(p.x + t * (q.x - p.x), p.y + t * (q.y - p.y)));
      }
    }
    subject = std::move(out);
  }
  return subject;
}

bool
Path2D::intersect(const Path2D & other) {
  auto as = toPolygons(*this), bs = toPolygons(other);
  std::vector<Polygon> result;
  for (auto & a : as) {
    for (auto & b : bs) {
      Polygon r;
      if (isConvex(b)) r = clipConvex(a, b);
      else if (isConvex(a)) r = clipConvex(b, a);
      else return false;
      if (r.size() > 2 && std::fabs(signedArea(r)) > 0) result.push_back(std::move(r));
    }
  }
  clear();
  if (result.empty()) {
    moveTo(0, 0);
  } else {
    for (auto & poly : result) {
      moveTo(poly[0]);
      for (size_t i = 1; i < poly.size(); i++) lineTo(poly[i]);
      closePath();
    }
  }
  return true;
}

// The winding number method counts the number of times a polygon winds
// around the point. If the result is 0, the point is outside.
bool
Path2D::isInside(double x, double y) const {
  int wn = 0;
  for (auto & poly : toPolygons(*this)) {
    for (size_t i = 0; i < poly.size(); i++) {
      auto & v1 = poly[i];
      auto & v2 = poly[(i + 1) % poly.size()];
      if (v1.y <= y) {
	if (v2.y > y && cross(v1, v2, Point(x, y)) > 0) wn++; // upward crossing, point left of edge
      } else {
	if (v2.y <= y && cross(v1, v2, Point(x, y)) < 0) wn--; // downward crossing, point right of edge
      }
    }
  }
  return wn != 0;
}
