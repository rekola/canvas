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
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
