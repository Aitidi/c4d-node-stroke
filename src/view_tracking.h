#pragma once
#include <optional>
#include <vector>
namespace nodestroke {
struct ViewRecord { double zoom, offset_x, offset_y; };
struct ViewChange { ViewRecord before, after; };
// Match unchanged records without relying on undocumented dictionary order.
// A result is returned only when exactly one uniquely identifiable record changed.
std::optional<ViewChange> identify_view_change(const std::vector<ViewRecord>& before,
                                                const std::vector<ViewRecord>& after,
                                                double tolerance = 0.001);
}
