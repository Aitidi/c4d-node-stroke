#include "gesture.h"
#include <algorithm>
#include <cmath>

namespace nodestroke {
namespace {
double cross(Point a, Point b, Point c) {
  return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
bool on_segment(Point a, Point b, Point p) {
  constexpr double eps = 1e-9;
  return std::abs(cross(a,b,p)) <= eps &&
    p.x >= std::min(a.x,b.x)-eps && p.x <= std::max(a.x,b.x)+eps &&
    p.y >= std::min(a.y,b.y)-eps && p.y <= std::max(a.y,b.y)+eps;
}
bool segments_intersect(Point a, Point b, Point c, Point d) {
  constexpr double eps = 1e-9;
  const double ab_c=cross(a,b,c), ab_d=cross(a,b,d);
  const double cd_a=cross(c,d,a), cd_b=cross(c,d,b);
  if (((ab_c>eps && ab_d< -eps) || (ab_c< -eps && ab_d>eps)) &&
      ((cd_a>eps && cd_b< -eps) || (cd_a< -eps && cd_b>eps))) return true;
  return on_segment(a,b,c) || on_segment(a,b,d) || on_segment(c,d,a) || on_segment(c,d,b);
}
}
bool intersects(const std::vector<Point>& gesture, const std::vector<Point>& wire) {
  for (std::size_t i=1; i<gesture.size(); ++i)
    for (std::size_t j=1; j<wire.size(); ++j)
      if (segments_intersect(gesture[i-1],gesture[i],wire[j-1],wire[j])) return true;
  return false;
}
bool is_shake(const std::vector<Point>& path, double minimum_excursion, unsigned minimum_reversals) {
  if (path.size()<2 || minimum_excursion<=0 || minimum_reversals==0) return false;
  double pivot=path.front().x, extreme=pivot;
  int direction=0;
  unsigned reversals=0;
  for (const Point& p : path) {
    if (direction==0) {
      if (std::abs(p.x-pivot)>=minimum_excursion) { direction=p.x>pivot ? 1 : -1; extreme=p.x; }
    } else if ((direction>0 && p.x>extreme) || (direction<0 && p.x<extreme)) {
      extreme=p.x;
    } else if ((direction>0 && extreme-p.x>=minimum_excursion) ||
               (direction<0 && p.x-extreme>=minimum_excursion)) {
      direction=-direction; extreme=p.x;
      if (++reversals>=minimum_reversals) return true;
    }
  }
  return false;
}
}
