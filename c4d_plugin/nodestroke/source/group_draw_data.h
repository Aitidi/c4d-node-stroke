#pragma once

// Internal C4D 2027.0 drawlist format. The caller must verify the exact host
// binaries, copy a consistent drawlist snapshot and bind it to the graph view.
// This parser deliberately fails closed on unknown or ambiguous structures.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wire_group_draw {
struct Command { std::uint8_t bytes[64]{}; };
struct Circle { float x, y; std::string label; };
struct Panels { std::vector<Circle> left, right; };
struct Port { std::string id, name; };
struct MatchedPort { float x, y; std::size_t port; };

inline float FloatAt(const Command& c, std::size_t off) {
  float value; std::memcpy(&value, c.bytes + off, sizeof(value)); return value;
}

// The callback copies the rendered text identified by a DrawText command.
template <typename ReadLabel>
bool Parse(const std::vector<Command>& commands, ReadLabel readLabel, Panels& result, const char** why = nullptr) {
  result = {};
  auto fail = [&](const char* reason) { if (why) *why = reason; return false; };
  if (commands.empty() || commands.size() > 262144 || commands[0].bytes[0] != 1) return fail("no canvas background");
  const float width = FloatAt(commands[0], 16), height = FloatAt(commands[0], 20);
  if (FloatAt(commands[0], 8) != 0 || FloatAt(commands[0], 12) != 0 ||
      !std::isfinite(width) || !std::isfinite(height) || width <= 100 || height <= 100 ||
      width >= 16000 || height >= 16000) return fail("canvas size");
  struct Panel { std::size_t start; bool left; float edge; };
  std::vector<Panel> panels;
  for (std::size_t i = 0; i < commands.size(); ++i) {
    const auto& c = commands[i];
    if (c.bytes[0] != 1) continue;
    const float x = FloatAt(c, 8), y = FloatAt(c, 12), w = FloatAt(c, 16), h = FloatAt(c, 20);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)) continue;
    if (std::abs(y) > .01f || std::abs(h - height) > .01f || w <= 0 || w >= width * .4f) continue;
    if (std::abs(x) <= .01f) panels.push_back({i, true, x + w});
    else if (std::abs(x + w - width) <= .01f) panels.push_back({i, false, x});
  }
  if (panels.size() != 2 || panels[0].left == panels[1].left) return fail("side panels not found");
  for (std::size_t i = 0; i < panels.size(); ++i) {
    const auto& panel = panels[i];
    auto& out = panel.left ? result.left : result.right;
    float previousRadius = 0;
    const std::size_t end = i + 1 < panels.size() ? panels[i + 1].start : commands.size();
    for (std::size_t j = panel.start + 1; j < end; ++j) {
      const auto& c = commands[j];
      if (c.bytes[0] == 8 && !out.empty()) {
        // Text after an already labelled port is a folder heading of the panel.
        if (!out.back().label.empty()) continue;
        // A port's label sits on its row: the text box starts about 7 px above the dot.
        const float y = FloatAt(c, 12);
        std::string label;
        if (!std::isfinite(y) || y < out.back().y - 12 || y > out.back().y + 2) return fail("label off the port row");
        if (!readLabel(c, label) || label.empty()) return fail("unreadable label");
        out.back().label = std::move(label);
      } else if (c.bytes[0] == 3) {
        const float x = FloatAt(c, 8), y = FloatAt(c, 12), rx = FloatAt(c, 16), ry = FloatAt(c, 20);
        if (std::isfinite(x) && std::isfinite(y) && std::isfinite(rx) && std::isfinite(ry) &&
            std::abs(x - panel.edge) < .01f && y >= 0 && y <= height && rx >= 1 && rx <= 20 &&
            std::abs(rx - ry) < .01f) {
          // Selected or highlighted ports are drawn as several concentric
          // circles before their single label, not as separate ports.
          if (!out.empty() && out.back().label.empty() && std::abs(y - out.back().y) < .01f && rx < previousRadius) {
            previousRadius = rx;
            continue;
          }
          if (!out.empty() && (out.back().label.empty() || y <= out.back().y)) return fail("port dot without label or out of order");
          out.push_back({x, y, {}});
          previousRadius = rx;
        }
      }
    }
    if (out.empty() || out.back().label.empty()) return fail("panel without labelled ports");
  }
  return true;
}

// A node as drawn: its body rectangle (below the title) and the port dots on its
// left (inputs) and right (outputs) edges with their labels, top to bottom.
struct DrawnNode { float x, y, w, h, radius; std::vector<Circle> ins, outs; };

// Node bodies are filled rectangles whose corners are rounded by 8 * zoom; their
// port dots have radius 5 * zoom and sit exactly on the body's side edges. A body
// may be followed by a shorter rectangle at the same origin (its icon bar), which is
// not a node. Port labels follow their dot on the same row; text after a labelled
// dot is a port group heading. Nodes whose drawing cannot be read are left out.
template <typename ReadLabel>
std::vector<DrawnNode> ParseNodes(const std::vector<Command>& commands, ReadLabel readLabel) {
  std::vector<DrawnNode> nodes;
  for (std::size_t i = 0; i < commands.size(); ++i) {
    const auto& c = commands[i];
    if (c.bytes[0] != 1) continue;
    const float x = FloatAt(c, 8), y = FloatAt(c, 12), w = FloatAt(c, 16), h = FloatAt(c, 20);
    // A text background is directly followed by its text at the same x.
    if (i + 1 < commands.size() && commands[i + 1].bytes[0] == 8 && std::abs(FloatAt(commands[i + 1], 8) - x) < .01f) continue;
    const float rx = FloatAt(c, 24), ry = FloatAt(c, 28);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h) || !std::isfinite(rx) ||
        rx < .5f || std::abs(rx - ry) > .01f || w <= 2 * rx || h <= 2 * rx) continue;
    bool merged = false;
    for (auto& n : nodes) {
      if (std::abs(n.x - x) > .01f || std::abs(n.y - y) > .01f || std::abs(n.w - w) > .01f) continue;
      n.h = std::max(n.h, h);
      merged = true;
    }
    if (!merged) nodes.push_back({x, y, w, h, rx, {}, {}});
  }
  std::vector<Circle>* last = nullptr;
  for (const auto& c : commands) {
    if (c.bytes[0] == 8 && last && !last->empty() && last->back().label.empty()) {
      const float y = FloatAt(c, 12);
      std::string label;
      if (std::isfinite(y) && y >= last->back().y - 12 && y <= last->back().y + 2 && readLabel(c, label)) last->back().label = label;
      last = nullptr;
      continue;
    }
    if (c.bytes[0] != 3) continue;
    const float x = FloatAt(c, 8), y = FloatAt(c, 12), rx = FloatAt(c, 16), ry = FloatAt(c, 20);
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(rx - ry) > .01f) continue;
    for (auto& n : nodes) {
      if (std::abs(rx - n.radius * .625f) > .02f || y < n.y || y > n.y + n.h) continue;
      std::vector<Circle>* side = std::abs(x - n.x) <= .05f ? &n.ins : std::abs(x - n.x - n.w) <= .05f ? &n.outs : nullptr;
      if (!side) continue;
      // Selected or highlighted ports repeat their dot; keep one per row.
      if (!side->empty() && std::abs(side->back().y - y) < .01f) { last = side; break; }
      side->push_back({x, y, {}});
      last = side;
      break;
    }
  }
  return nodes;
}

// Pairs drawn ports with ports listed in drawing order. Every label must equal the
// label at the same position, so same-named ports can be told apart by order.
inline bool MatchInOrder(const std::vector<Circle>& circles, const std::vector<Port>& ports,
                         std::vector<MatchedPort>& result) {
  result.clear();
  if (circles.empty() || circles.size() != ports.size()) return false;
  for (std::size_t i = 0; i < ports.size(); ++i) {
    if (circles[i].label.empty() || circles[i].label != ports[i].name) { result.clear(); return false; }
    result.push_back({circles[i].x, circles[i].y, i});
  }
  return true;
}

// Pairs drawn ports with listed ports in order, where some listed ports were not
// drawn. Accepted only when the alignment is unique: the leftmost and rightmost
// assignment of every drawn label agree.
inline bool MatchSubsequence(const std::vector<Circle>& circles, const std::vector<Port>& ports,
                             std::vector<MatchedPort>& result) {
  result.clear();
  if (circles.empty() || circles.size() > ports.size()) return false;
  std::vector<std::size_t> left(circles.size()), right(circles.size());
  std::size_t next = 0;
  for (std::size_t i = 0; i < circles.size(); ++i) {
    while (next < ports.size() && ports[next].name != circles[i].label) ++next;
    if (next == ports.size() || circles[i].label.empty()) return false;
    left[i] = next++;
  }
  std::size_t end = ports.size();
  for (std::size_t i = circles.size(); i-- > 0;) {
    while (end > 0 && ports[end - 1].name != circles[i].label) --end;
    if (end == 0) return false;
    right[i] = --end;
  }
  if (left != right) return false;
  for (std::size_t i = 0; i < circles.size(); ++i) result.push_back({circles[i].x, circles[i].y, left[i]});
  return true;
}

// Pairs drawn ports with a subset of the listed ports (some may sit in a collapsed
// port group) by full label. A label must belong to exactly one listed port.
inline bool MatchSubset(const std::vector<Circle>& circles, const std::vector<Port>& ports,
                        std::vector<MatchedPort>& result) {
  result.clear();
  if (circles.empty() || circles.size() > ports.size()) return false;
  std::vector<bool> used(ports.size(), false);
  for (const auto& circle : circles) {
    std::size_t found = ports.size();
    for (std::size_t i = 0; i < ports.size(); ++i) {
      if (circle.label.empty() || ports[i].name != circle.label) continue;
      if (found != ports.size()) { result.clear(); return false; }
      found = i;
    }
    if (found == ports.size() || used[found]) { result.clear(); return false; }
    used[found] = true;
    result.push_back({circle.x, circle.y, found});
  }
  return true;
}

inline bool Match(const std::vector<Circle>& circles, const std::vector<Port>& ports,
                  std::vector<MatchedPort>& result) {
  result.clear();
  if (circles.empty() || circles.size() != ports.size()) return false;
  std::unordered_map<std::string, std::size_t> byName;
  std::unordered_set<std::string> ids;
  for (std::size_t i = 0; i < ports.size(); ++i) {
    if (ports[i].id.empty() || ports[i].name.empty() ||
        !ids.insert(ports[i].id).second || !byName.emplace(ports[i].name, i).second) return false;
  }
  std::vector<bool> used(ports.size(), false);
  for (const auto& circle : circles) {
    if (circle.label.empty()) { result.clear(); return false; }
    const auto foundEntry = byName.find(circle.label);
    if (foundEntry == byName.end()) { result.clear(); return false; }
    const std::size_t found = foundEntry->second;
    if (used[found]) { result.clear(); return false; }
    used[found] = true;
    result.push_back({circle.x, circle.y, found});
  }
  return true;
}
}
