// Backend-independent Path2D tests.
#include <Path2D.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace canvas;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bool at(const Point & p, double x, double y) { return std::abs(p.x - x) < 1e-9 && std::abs(p.y - y) < 1e-9; }

int main() {
  {  // closePath() returns to the start of the current subpath.
    Path2D path;
    path.rect(0, 0, 4, 4);
    path.rect(8, 8, 4, 4);
    CHECK(at(path.getCurrentPoint(), 8, 8));
    path.moveTo(0, 12);
    path.lineTo(4, 12);
    path.lineTo(4, 16);
    path.closePath();
    CHECK(at(path.getCurrentPoint(), 0, 12));
  }
  {  // An arc that starts a path starts its subpath at the arc's start.
    Path2D path;
    path.arc(Point(10, 10), 5, 0, M_PI / 2, false);
    CHECK(at(path.getCurrentPoint(), 10, 15));
    path.closePath();
    CHECK(at(path.getCurrentPoint(), 15, 10));
  }
  {  // Extents cover every point.
    Path2D path;
    path.moveTo(3, -2);
    path.lineTo(-1, 7);
    path.lineTo(5, 4);
    path.closePath();
    double x0, y0, x1, y1;
    path.getExtents(x0, y0, x1, y1);
    CHECK(x0 == -1 && y0 == -2 && x1 == 5 && y1 == 7);
  }
  {  // An arc's extents cover its circle.
    Path2D path;
    path.arc(Point(10, 20), 5, 0, 2 * M_PI, false);
    double x0, y0, x1, y1;
    path.getExtents(x0, y0, x1, y1);
    CHECK(x0 == 5 && y0 == 15 && x1 == 15 && y1 == 25);
  }
  {  // An empty path has empty extents.
    Path2D path;
    double x0 = 1, y0 = 1, x1 = 1, y1 = 1;
    path.getExtents(x0, y0, x1, y1);
    CHECK(x0 == 0 && y0 == 0 && x1 == 0 && y1 == 0);
  }
  {  // offset() moves the current point with the path.
    Path2D path;
    path.moveTo(1, 2);
    path.lineTo(3, 4);
    path.offset(10, 20);
    CHECK(at(path.getCurrentPoint(), 13, 24));
    path.closePath();
    CHECK(at(path.getCurrentPoint(), 11, 22));
  }
  {  // isInside(): subpaths close on their own start, and a moveTo isn't an edge.
    Path2D path;
    path.rect(0, 0, 4, 4);
    path.rect(10, 10, 4, 4);
    CHECK(path.isInside(2, 2) && path.isInside(12, 12));
    CHECK(!path.isInside(7, 7) && !path.isInside(2, 12) && !path.isInside(12, 2));
  }
  {  // isInside() sees arcs.
    Path2D path;
    path.arc(Point(10, 10), 5, 0, 2 * M_PI, false);
    CHECK(path.isInside(10, 10) && path.isInside(13, 13) && !path.isInside(15, 15));
  }
  {  // A similarity keeps an arc: the scaled, rotated centre and swept angles.
    Path2D path;
    path.arc(Point(1, 0), 2, 0, M_PI / 2, false);
    TransformationMatrix m(0, 2, -2, 0, 5, 5);  // rotate 90 degrees, scale 2, translate
    auto t = path.transform(m);
    auto & pc = t.getData()[0];
    CHECK(pc.type == PathComponent::ARC);
    CHECK(std::abs(pc.x0 - 5) < 1e-9 && std::abs(pc.y0 - 7) < 1e-9 && std::abs(pc.radius - 4) < 1e-9);
    CHECK(std::abs(pc.sa - M_PI / 2) < 1e-9 && std::abs(pc.ea - M_PI) < 1e-9 && !pc.anticlockwise);
  }
  {  // A mirror flips the arc's direction.
    Path2D path;
    path.arc(Point(0, 0), 1, 0, M_PI / 2, false);
    auto t = path.transform(TransformationMatrix(-1, 0, 0, 1, 0, 0));
    auto & pc = t.getData()[0];
    CHECK(pc.anticlockwise && std::abs(pc.sa - M_PI) < 1e-9 && std::abs(pc.ea - M_PI / 2) < 1e-9);
  }
  {  // A non-uniform scale flattens an arc into the ellipse's points.
    Path2D path;
    path.arc(Point(0, 0), 1, 0, 2 * M_PI, false);
    auto t = path.transform(TransformationMatrix(4, 0, 0, 1, 0, 0));
    CHECK(t.getData()[0].type == PathComponent::MOVE_TO);
    double x0, y0, x1, y1;
    t.getExtents(x0, y0, x1, y1);
    CHECK(std::abs(x1 - 4) < 1e-9 && std::abs(y1 - 1) < 1e-9);
  }
  {  // Clips intersect; disjoint clips leave nothing visible but are not "no clip".
    Path2D a, b;
    a.rect(0, 0, 10, 10);
    b.rect(5, 5, 10, 10);
    CHECK(a.intersect(b));
    CHECK(a.isInside(7, 7) && !a.isInside(2, 2) && !a.isInside(12, 12));
    Path2D c;
    c.rect(20, 20, 2, 2);
    CHECK(a.intersect(c));
    CHECK(!a.empty() && !a.isInside(21, 21) && !a.isInside(7, 7));
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
