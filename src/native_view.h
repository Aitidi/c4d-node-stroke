#pragma once
#include "gesture.h"
#include "view_tracking.h"
#include <optional>
namespace nodestroke {
// Converts a graph-space point to screen space using a measured editor-canvas
// origin and one identified view record. Only the mathematical transform is
// provided here; callers must independently validate view identity and origin.
std::optional<Point> graph_to_screen(Point node, Point canvas_origin, const ViewRecord& view);
std::optional<Point> screen_to_graph(Point screen, Point canvas_origin, const ViewRecord& view);
}
