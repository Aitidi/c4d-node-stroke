#include "graph.h"
#include <algorithm>

namespace nodestroke {
namespace {
const Node* find_node(const Graph& g, const std::string& id) {
  for (const auto& n : g.nodes) if (n.id == id) return &n;
  return nullptr;
}
const Port* port(const std::vector<Port>& ports, const std::string& id) {
  for (const auto& p : ports) if (p.id == id) return &p;
  return nullptr;
}
bool compatible(const Port& a, const Port& b) { return a.type == b.type; }
bool connect(Graph& g, const Hit& from, const Hit& to, const std::vector<Rule>& rules) {
  const Node* a = find_node(g, from.node); const Node* b = find_node(g, to.node);
  if (!a || !b || a->id == b->id) return false;
  const Port* out = from.port.empty() ? nullptr : port(a->outputs, from.port);
  const Port* in = to.port.empty() ? nullptr : port(b->inputs, to.port);
  if ((!from.port.empty() && !out) || (!to.port.empty() && !in)) return false;
  // Explicit ports take precedence over custom rules; missing sides may use a rule.
  for (const auto& r : rules) {
    if (r.source_asset != a->asset || r.target_asset != b->asset) continue;
    const Port* ro = port(a->outputs, r.source_port); const Port* ri = port(b->inputs, r.target_port);
    if ((!out || out == ro) && (!in || in == ri) && ro && ri && compatible(*ro,*ri)) { out=ro; in=ri; break; }
  }
  if (!out || !in) {
    for (const auto& candidate_out : a->outputs) {
      if (out && out != &candidate_out) continue;
      if (!out && !candidate_out.visible) continue;
      for (const auto& candidate_in : b->inputs) {
        if (in && in != &candidate_in) continue;
        if (!in && !candidate_in.visible) continue;
        if (compatible(candidate_out,candidate_in)) { out=&candidate_out; in=&candidate_in; break; }
      }
      if (out && in) break;
    }
  }
  if (!out || !in || !compatible(*out,*in)) return false;
  g.links.erase(std::remove_if(g.links.begin(),g.links.end(),[&](const Link& l) {
    return l.target_node==b->id && l.target_port==in->id;
  }),g.links.end());
  g.links.push_back({a->id,out->id,b->id,in->id});
  return true;
}
bool same(const Link& a,const Link& b) {
 return a.source_node==b.source_node && a.source_port==b.source_port && a.target_node==b.target_node && a.target_port==b.target_port;
}
}
bool draw(Graph& graph, const std::vector<Hit>& hits, const std::vector<Rule>& rules, bool fan_out) {
  if (hits.size()<2) return false;
  Graph next=graph;
  for (std::size_t i=1;i<hits.size();++i)
    if (!connect(next, hits[fan_out ? 0 : i-1], hits[i], rules)) return false;
  graph=std::move(next); return true;
}
bool insert(Graph& graph, const Link& wire, const std::string& node, const std::vector<Rule>& rules) {
  Graph next=graph;
  auto it=std::find_if(next.links.begin(),next.links.end(),[&](const Link& l){return same(l,wire);});
  if (it==next.links.end() || node==wire.source_node || node==wire.target_node) return false;
  next.links.erase(it);
  if (!connect(next,{wire.source_node,wire.source_port},{node,{}},rules) ||
      !connect(next,{node,{}},{wire.target_node,wire.target_port},rules)) return false;
  graph=std::move(next); return true;
}
void disconnect(Graph& graph, const std::string& node) {
  graph.links.erase(std::remove_if(graph.links.begin(),graph.links.end(),[&](const Link& l){return l.source_node==node || l.target_node==node;}),graph.links.end());
}
std::size_t cut(Graph& graph, const std::vector<DisplayWire>& wires, const std::vector<Point>& stroke) {
  const auto before=graph.links.size();
  graph.links.erase(std::remove_if(graph.links.begin(),graph.links.end(),[&](const Link& link) {
    for (const auto& wire : wires) if (same(link,wire.link) && intersects(stroke,wire.path)) return true;
    return false;
  }),graph.links.end());
  return before-graph.links.size();
}
}

