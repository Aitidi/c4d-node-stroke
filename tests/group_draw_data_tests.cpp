#include "../c4d_plugin/nodestroke/source/group_draw_data.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using namespace wire_group_draw;
static Command Make(std::uint8_t type, float x, float y, float w, float h) {
  Command c{}; c.bytes[0] = type;
  std::memcpy(c.bytes + 8, &x, 4); std::memcpy(c.bytes + 12, &y, 4);
  std::memcpy(c.bytes + 16, &w, 4); std::memcpy(c.bytes + 20, &h, 4);
  return c;
}
int main() {
  std::vector<Command> commands{Make(1,0,0,1000,500), Make(1,0,0,40,500)};
  std::vector<Port> ports;
  for (int i = 0; i < 200; ++i) {
    commands.push_back(Make(3,40,static_cast<float>(i * 2),5,5));
    Command text = Make(8,0,static_cast<float>(i * 2) - 7,0,0); text.bytes[1] = static_cast<std::uint8_t>(i); commands.push_back(text);
    ports.push_back({std::to_string(i), std::to_string(i)});
  }
  commands.push_back(Make(1,950,0,50,500));
  commands.push_back(Make(3,950,10,5,5));
  commands.push_back(Make(3,950,10,3,3));
  commands.push_back(Make(3,950,10,1.5f,1.5f));
  commands.push_back(Make(8,0,3,0,0));
  Panels panels;
  auto label = [](const Command& c, std::string& s) { s = std::to_string(c.bytes[1]); return true; };
  assert(Parse(commands, label, panels));
  assert(panels.left.size() == 200 && panels.right.size() == 1);
  std::reverse(ports.begin(), ports.end());
  std::vector<MatchedPort> matched;
  assert(Match(panels.left, ports, matched));
  assert(matched.size() == 200 && ports[matched[0].port].id == "0");
  ports[0].name = ports[1].name;
  assert(!Match(panels.left, ports, matched) && matched.empty());
  commands.push_back(Make(1,0,0,50,500));
  assert(!Parse(commands, label, panels));

  // Folder headings between ports are skipped; same-named ports pair up by order.
  auto named = [](const Command& c, std::string& s) { s = c.bytes[1] ? "Color" : "Bool"; return true; };
  auto text = [](float y, std::uint8_t name) { Command c = Make(8,0,y,0,0); c.bytes[1] = name; return c; };
  std::vector<Command> folders{Make(1,0,0,1000,500), Make(1,0,0,80,500),
    Make(3,80,10,5,5), text(3,0), text(43,1), Make(3,80,70,5,5), text(63,1),
    Make(1,950,0,50,500), Make(3,950,10,5,5), text(3,1), Make(3,950,30,5,5), text(23,1)};
  assert(Parse(folders, named, panels));
  assert(panels.left.size() == 2 && panels.left[1].y == 70 && panels.left[1].label == "Color");
  std::vector<Port> outputs{{"color1", "Color"}, {"outcolor", "Color"}};
  assert(!Match(panels.right, outputs, matched));
  assert(MatchInOrder(panels.right, outputs, matched) && matched[0].port == 0 && matched[1].y == 30);
  outputs[1].name = "Other";
  assert(!MatchInOrder(panels.right, outputs, matched) && matched.empty());
  // A label off the dot's row is not taken as that port's label.
  folders[3] = text(40,0);
  assert(!Parse(folders, named, panels));
  // Nodes at zoom 0.72: body rounded by 5.76, its icon bar at the same origin,
  // port dots of radius 3.6 on the edges, labels on the dot's row, a group heading.
  auto rounded = [](float x, float y, float w, float h, float r) {
    Command c = Make(1, x, y, w, h); std::memcpy(c.bytes + 24, &r, 4); std::memcpy(c.bytes + 28, &r, 4); return c;
  };
  auto port = [](const Command& c, std::string& s) { s = "p" + std::to_string(c.bytes[1]); return true; };
  auto row = [](float y, std::uint8_t name) { Command c = Make(8,0,y,0,0); c.bytes[1] = name; return c; };
  std::vector<Command> canvas{Make(1,0,0,1000,800),
    rounded(100,100,92.21f,150,5.76f), rounded(100,100,92.21f,17.29f,5.76f),
    Make(3,192.21f,130,3.6f,3.6f), row(123,0),            // output p0
    row(138,9),                                            // heading after a labelled port
    Make(3,100,160,3.6f,3.6f), Make(3,118.7f,160,1.08f,1.08f), row(153,1),  // input p1, inner dot ignored
    Make(3,100,175,3.6f,3.6f), row(168,2),                 // input p2
    rounded(400,100,92.21f,60,5.76f), Make(3,400,130,3.6f,3.6f), row(123,1),
    rounded(1579,700,279,90,8)};                             // minimap: no dots on its edges
  const auto nodes = ParseNodes(canvas, port);
  assert(nodes.size() == 3);
  assert(nodes[0].h == 150 && nodes[0].outs.size() == 1 && nodes[0].outs[0].label == "p0");
  assert(nodes[0].ins.size() == 2 && nodes[0].ins[0].label == "p1" && nodes[0].ins[1].y == 175 && nodes[0].ins[1].label == "p2");
  assert(nodes[1].ins.size() == 1 && nodes[1].ins[0].label == "p1" && nodes[2].ins.empty() && nodes[2].outs.empty());
  // A collapsed port group hides some ports: the drawn ones pair up by unique label.
  std::vector<Port> inputs{{"", "p3"}, {"", "p1"}, {"", "p2"}};
  assert(!MatchInOrder(nodes[0].ins, inputs, matched));
  assert(MatchSubset(nodes[0].ins, inputs, matched) && matched[0].port == 1 && matched[1].port == 2);
  inputs[0].name = "p1";
  assert(!MatchSubset(nodes[0].ins, inputs, matched) && matched.empty());
  // Text backgrounds (rounded, followed by their text) are not node bodies.
  std::vector<Command> texts{Make(1,0,0,1000,800), rounded(300,300,64,60,4), rounded(310,320,40,14,4), Make(8,310,322,40,1)};
  assert(ParseNodes(texts, port).size() == 1);
  // Repeated labels pair up by order when only one alignment fits.
  std::vector<Circle> layers{{0,1,"Color"}, {0,2,"Color"}, {0,3,"Mask"}};
  std::vector<Port> layerPorts{{"", "Alpha"}, {"", "Color"}, {"", "Color"}, {"", "Mask"}};
  assert(MatchSubsequence(layers, layerPorts, matched) && matched[0].port == 1 && matched[2].port == 3);
  layerPorts.insert(layerPorts.begin() + 1, Port{"", "Color"});
  assert(!MatchSubsequence(layers, layerPorts, matched) && matched.empty());
  std::cout << "group draw data tests passed\n";
}
