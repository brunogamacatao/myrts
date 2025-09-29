#include <vector>
#include <deque>

struct Point {
  int x, y;

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }

  bool operator<(const Point& other) const {
    return x == other.x && y == other.y;
  }
};

std::deque<Point> a_star(const std::vector<std::vector<int>>& grid, const Point& start, const Point& goal);
