#include "native_view.h"
#include <cmath>
namespace nodestroke {
namespace {
bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool valid(const ViewRecord& v) { return std::isfinite(v.zoom) && v.zoom>0 && std::isfinite(v.offset_x) && std::isfinite(v.offset_y); }
}
std::optional<Point> graph_to_screen(Point node, Point origin, const ViewRecord& v) {
 if (!finite(node) || !finite(origin) || !valid(v)) return {};
 Point p{origin.x+v.offset_x+v.zoom*node.x,origin.y+v.offset_y+v.zoom*node.y};
 return finite(p) ? std::optional<Point>(p) : std::nullopt;
}
std::optional<Point> screen_to_graph(Point screen, Point origin, const ViewRecord& v) {
 if (!finite(screen) || !finite(origin) || !valid(v)) return {};
 Point p{(screen.x-origin.x-v.offset_x)/v.zoom,(screen.y-origin.y-v.offset_y)/v.zoom};
 return finite(p) ? std::optional<Point>(p) : std::nullopt;
}
}
