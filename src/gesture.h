#pragma once
#include <vector>

namespace nodestroke {
struct Point { double x; double y; };
// Tests a gesture against a polyline approximating a displayed wire.
// Endpoints and collinear overlap count as intersections.
bool intersects(const std::vector<Point>& gesture, const std::vector<Point>& wire);
// Detect alternating horizontal movement with a minimum excursion per reversal.
bool is_shake(const std::vector<Point>& path, double minimum_excursion, unsigned minimum_reversals);
}
