#pragma once
#include "gesture.h"
#include <string>
#include <vector>

namespace nodestroke {
struct Port { std::string id; std::string type; bool visible = true; };
struct Node { std::string id; std::string asset; std::vector<Port> inputs, outputs; };
struct Link { std::string source_node, source_port, target_node, target_port; };
struct DisplayWire { Link link; std::vector<Point> path; };
struct Rule { std::string source_asset, target_asset, source_port, target_port; };
struct Hit { std::string node; std::string port; };
struct Graph { std::vector<Node> nodes; std::vector<Link> links; };
// Operates on a copy; on failure the graph is unchanged. Adapter must additionally
// validate host types and commit atomically in a Cinema 4D graph transaction.
bool draw(Graph& graph, const std::vector<Hit>& hits, const std::vector<Rule>& rules, bool fan_out);
bool insert(Graph& graph, const Link& wire, const std::string& node, const std::vector<Rule>& rules);
void disconnect(Graph& graph, const std::string& node);
std::size_t cut(Graph& graph, const std::vector<DisplayWire>& wires, const std::vector<Point>& stroke);
}

