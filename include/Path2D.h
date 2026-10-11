#ifndef _CANVAS_PATH2D_H_
#define _CANVAS_PATH2D_H_

#include <Point.h>
#include <TransformationMatrix.h>

#include <cmath>
#include <vector>

namespace canvas {
  class PathComponent {
  public:
    enum Type { MOVE_TO = 1, LINE_TO, ARC, CLOSE };

  PathComponent(Type _type) : type(_type), x0(0), y0(0), radius(0), sa(0), ea(0), anticlockwise(false) { }
  PathComponent(Type _type, double _x0, double _y0) : type(_type), x0(_x0), y0(_y0), radius(0), sa(0), ea(0), anticlockwise(false) { }
  PathComponent(Type _type, double _x0, double _y0, double _radius, double _sa, double _ea, bool _anticlockwise) : type(_type), x0(_x0), y0(_y0), radius(_radius), sa(_sa), ea(_ea), anticlockwise(_anticlockwise) { }
      
    Type type;
    double x0, y0, radius, sa, ea;
    bool anticlockwise;
  };
  
  class Path2D {
  public:
    Path2D() : current_point_(0.0, 0.0), subpath_start_(0.0, 0.0) { }
    
    void moveTo(double x, double y) {
      data_.push_back(PathComponent(PathComponent::MOVE_TO, x, y));
      current_point_ = subpath_start_ = Point(x, y);
    }
    void moveTo(Point p) {
      moveTo(p.x, p.y);
    }
    void lineTo(double x, double y) {
      data_.push_back(PathComponent(PathComponent::LINE_TO, x, y));
      current_point_ = Point(x, y);
    }
    void lineTo(Point p) {
      data_.push_back(PathComponent(PathComponent::LINE_TO, p.x, p.y));
      current_point_ = std::move(p);
    }
    // Back to the start of the current subpath, not of the whole path.
    void closePath() {
      if (!data_.empty()) {
	data_.push_back(PathComponent(PathComponent::CLOSE));
	current_point_ = subpath_start_;
      }
    }
    void arc(const Point & p, double radius, double sa, double ea, bool anticlockwise);
    void arcTo(const Point & p1, const Point & p2, double radius);

    void rect(double x, double y, double w, double h) {
      moveTo(x, y);
      lineTo(x + w, y);
      lineTo(x + w, y + h);
      lineTo(x, y + h); 
      closePath();
    }

    const std::vector<PathComponent> & getData() const { return data_; }

    void clear() {
      data_.clear();
      current_point_ = subpath_start_ = Point(0, 0);
    }

    const Point & getCurrentPoint() const { return current_point_; }

    void offset(double dx, double dy) {
      for (auto & pc : data_) {
	pc.x0 += dx;
	pc.y0 += dy;
      }
      current_point_.x += dx;
      current_point_.y += dy;
      subpath_start_.x += dx;
      subpath_start_.y += dy;
    }

    void getExtents(double & min_x, double & min_y, double & max_x, double & max_y) const {
      if (data_.empty()) {
	min_x = min_y = max_x = max_y = 0;
      } else {
	min_x = min_y = HUGE_VAL;
	max_x = max_y = -HUGE_VAL;
	for (auto & pc : data_) {
	  if (pc.type == PathComponent::CLOSE) continue;
	  // An arc is bounded by its whole circle.
	  double r = pc.type == PathComponent::ARC ? pc.radius : 0.0;
	  if (pc.x0 - r < min_x) min_x = pc.x0 - r;
	  if (pc.y0 - r < min_y) min_y = pc.y0 - r;
	  if (pc.x0 + r > max_x) max_x = pc.x0 + r;
	  if (pc.y0 + r > max_y) max_y = pc.y0 + r;
	}
      }
    }

    bool empty() const { return data_.empty(); }
    bool isInside(double x, double y) const;
    std::size_t size() const { return data_.size(); }

    Path2D transform(const TransformationMatrix & matrix) const {
      Path2D new_path;
      for (auto pc : data_) {
	auto p = matrix.multiply(pc.x0, pc.y0);
	pc.x0 = p.x;
	pc.y0 = p.y;
#if 0
	pc.sa = matrix.transformAngle(pc.sa);
	pc.ea = matrix.transformAngle(pc.ea);
#endif
	pc.radius = matrix.transformSize(pc.radius);
	new_path.data_.push_back(std::move(pc));
      }
      new_path.current_point_ = matrix.multiply(current_point_);
      new_path.subpath_start_ = matrix.multiply(subpath_start_);
      return new_path;
    }

    Path2D flatten() const {
      Path2D new_path;
      for (auto pc : data_) {
	if (pc.type == PathComponent::ARC) {
	  double ea = pc.ea, step;
	  if (pc.anticlockwise) {
	    while (ea > pc.sa) ea -= 2*M_PI;
	    step = (ea - pc.sa) / 64.0;
	  } else {
	    while (ea < pc.sa) ea += 2*M_PI;
	    step = (ea - pc.sa) / 64.0;
	  }
	  double a = pc.sa;
	  for (int i = 0; i <= 64; i++, a += step) {
	    auto x = pc.x0 + cos(a) * pc.radius;
	    auto y = pc.y0 + sin(a) * pc.radius;
	    if (i == 0) new_path.moveTo(x, y);
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
    
  private:
    std::vector<PathComponent> data_;
    Point current_point_, subpath_start_;
  };
};

#endif
