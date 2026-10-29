#include "gesture.h"
#include <cstdlib>
#include <iostream>
using namespace nodestroke;
void check(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
int main() {
  check(intersects({{0,0},{10,10}},{{0,10},{10,0}}), "crossing");
  check(intersects({{0,0},{5,0}},{{5,0},{10,0}}), "endpoint");
  check(intersects({{0,0},{10,0}},{{3,0},{7,0}}), "overlap");
  check(!intersects({{0,0},{1,0}},{{2,0},{3,0}}), "disjoint");
  check(!intersects({},{{0,0},{1,1}}), "empty");
  check(is_shake({{0,0},{12,0},{-2,0},{13,0},{-3,0}},10,3), "shake");
  check(!is_shake({{0,0},{12,0},{8,0},{15,0},{12,0}},10,2), "noise");
  check(!is_shake({{0,0},{0,30},{0,-30}},10,1), "vertical");
}
