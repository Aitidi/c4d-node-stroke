// Read-only host verification for the native drawlist capture. This temporary
// research DLL is not shipped with the plugin or registered with C4D.
#include <map>
#include <cmath>
#include "../c4d_plugin/nodestroke/source/group_native_access.h"
extern "C" __declspec(dllexport) int probe_group_capture(void* hwnd, int* counts) {
  if (!counts) return 0;
  wire_group_native::Snapshot snapshot;
  if (!wire_group_native::Capture(static_cast<HWND>(hwnd), snapshot)) return 0;
  counts[0] = static_cast<int>(snapshot.panels.left.size());
  counts[1] = static_cast<int>(snapshot.panels.right.size());
  return 1;
}
extern "C" __declspec(dllexport) void probe_group_steps(void* hwnd, unsigned long long* values) {
  using namespace wire_group_native;
  if (!values) return;
  for (int i = 0; i < 10; ++i) values[i] = 0;
  values[0] = SupportedBuild();
  if (!values[0]) return;
  const Address os = WindowObject(static_cast<HWND>(hwnd)); values[1] = os;
  if (!os) return;
  const Address window = FindWindow(os); values[2] = window;
  if (!window) return;
  const Address list = Word(window + 0x58), app = Word(window + 0x548);
  values[3] = list; values[4] = app;
  values[5] = ClassIs(list, "net.maxon.mvp.gluon2.drawlist");
  values[6] = ClassIs(app, "net.maxon.mvp.presenterclasses.app");
  values[7] = Word(list + 0xe0 + 0x48); values[8] = Word(list + 0xe0 + 0x58);
  Snapshot snapshot; values[9] = Capture(static_cast<HWND>(hwnd), snapshot);
}
// Writes the failure reason and, when the drawlist can be read, every circle and
// label command after the first side panel as text lines into `out`.
extern "C" __declspec(dllexport) int probe_group_dump(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0) return 0;
  std::string text;
  Snapshot snapshot;
  const bool ok = Capture(static_cast<HWND>(hwnd), snapshot);
  text += ok ? "capture ok\n" : std::string("capture failed: ") + (snapshot.failure ? snapshot.failure : "?") + "\n";
  for (const auto* side : {&snapshot.panels.left, &snapshot.panels.right})
    for (const auto& c : *side) text += "port " + std::to_string(c.x) + "," + std::to_string(c.y) + " " + c.label + "\n";
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (window) {
    const Address list = Word(window + 0x58), component = list + 0xe0, chunks = Word(component + 0x30);
    const Address chunkCount = Word(component + 0x48);
    std::vector<wire_group_draw::Command> commands;
    struct Chunk { Address unused, commands; std::uint64_t count, capacity; } chunk{};
    for (Address part = 0; part < chunkCount && part < 256; ++part) {
      if (!ReadValue(chunks + part * 32, chunk) || chunk.count > 1024) break;
      for (std::uint64_t i = 0; i < chunk.count; ++i) {
        wire_group_draw::Command c{};
        if (ReadValue(chunk.commands + i * 64, c)) commands.push_back(c);
      }
    }
    const float width = commands.empty() ? 0 : wire_group_draw::FloatAt(commands[0], 16);
    for (std::size_t i = 0; i < commands.size(); ++i) {
      const auto& c = commands[i];
      const float x = wire_group_draw::FloatAt(c, 8), y = wire_group_draw::FloatAt(c, 12);
      const float w = wire_group_draw::FloatAt(c, 16), h = wire_group_draw::FloatAt(c, 20);
      const bool nearEdge = x < 120 || x > width - 120;
      if (!nearEdge && c.bytes[0] != 8) continue;
      std::string label;
      if (c.bytes[0] == 8 && !ReadLabel(c, label)) label = "<unreadable>";
      if (c.bytes[0] == 8 && !nearEdge && label.size() > 40) continue;
      text += std::to_string(i) + " t" + std::to_string(c.bytes[0]) + " " + std::to_string(x) + "," + std::to_string(y) + " " +
              std::to_string(w) + "x" + std::to_string(h) + (label.empty() ? "" : " '" + label + "'") + "\n";
      if (text.size() > std::size_t(size) - 256) break;
    }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return ok ? 1 : 0;
}
namespace {
std::string ClassName(wire_group_native::Address object) {
  using namespace wire_group_native;
  const Address info = Word(object);
  struct Header { Address a, b, name; std::uint64_t size; } header{};
  if (!object || !ReadValue(info, header) || header.size > 160) return "?";
  std::string name(static_cast<std::size_t>(header.size), '\0');
  return Read(header.name, name.data(), name.size()) ? name : "?";
}
void Walk(wire_group_native::Address node, int depth, std::string& text, int& budget) {
  using namespace wire_group_native;
  std::unordered_set<Address> seen;
  for (; node && seen.insert(node).second && --budget > 0; node = Word(node + 0xb8)) {
    text += std::string(depth * 2, ' ') + ClassName(node) + "\n";
    if (depth < 4) Walk(Word(node + 0xd0), depth + 1, text, budget);
  }
}
}
// Lists the presenter tree under the bound graph model: class names, first child
// at +0xd0, next sibling at +0xb8 (the layout PresentedPortsMatch relies on).
extern "C" __declspec(dllexport) int probe_presenter_tree(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address app = Word(window + 0x548), model = Word(app + 0xd0);
  if (!ClassIs(model, "net.maxon.mvp.presenterclasses.graphmodel")) return 0;
  std::string text;
  int budget = 400;
  Walk(Word(model + 0xd0), 0, text, budget);
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
namespace {
void WalkPorts(wire_group_native::Address p, int depth, wire_group_native::Address infoOffset, std::string& text, int& budget) {
  using namespace wire_group_native;
  std::unordered_set<Address> seen;
  for (; p && seen.insert(p).second && --budget > 0; p = Word(p + 0xb8)) {
    const Address ci = Word(p), size = Word(ci + 0x20);
    unsigned char valid = 0, code[5]{};
    Address fn = 0, delta = 0;
    const bool inRange = size > infoOffset;
    if (inRange) { ReadValue(ci + infoOffset, valid); ReadValue(ci + infoOffset + 1, fn); ReadValue(ci + infoOffset + 9, delta); }
    const bool codeOk = fn && Read(fn, code, sizeof(code)) && std::memcmp(code, "\x48\x8d\x41\x08\xc3", 5) == 0;
    char line[512];
    std::snprintf(line, sizeof(line), "%*s%s size=%llx inRange=%d valid=%d delta=%llu code=%d at180=%s\n", depth * 2, "",
      ClassName(p).c_str(), (unsigned long long)size, inRange, valid, (unsigned long long)delta, codeOk, ClassName(Word(p + 0x180)).c_str());
    text += line;
    if (depth < 6) WalkPorts(Word(p + 0xd0), depth + 1, infoOffset, text, budget);
  }
}
}
// For each graphionode port list: every child with the GetGraphNode accessor checks.
extern "C" __declspec(dllexport) int probe_presenter_ports(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address model = Word(Word(window + 0x548) + 0xd0);
  const Address module = reinterpret_cast<Address>(GetModuleHandleW(L"c4d_nodeeditor.xdl64"));
  const Address infoOffset = module ? Word(module + 0x28f750) : 0;
  std::string text = "infoOffset=" + std::to_string(infoOffset) + "\n";
  int budget = 400;
  std::unordered_set<Address> seen;
  for (Address node = Word(model + 0xd0); node && seen.insert(node).second; node = Word(node + 0xb8)) {
    if (!ClassIs(node, "net.maxon.mvp.presenterclasses.graphionode")) continue;
    WalkPorts(Word(node + 0xd0), 0, infoOffset, text, budget);
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Every drawlist command except grid lines: index, type, six floats, and the
// remaining bytes in hex, so unknown command layouts can be studied.
extern "C" __declspec(dllexport) int probe_draw_raw(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address list = Word(window + 0x58), component = list + 0xe0, chunks = Word(component + 0x30);
  const Address chunkCount = Word(component + 0x48);
  std::string text;
  struct Chunk { Address unused, commands; std::uint64_t count, capacity; } chunk{};
  std::size_t index = 0;
  for (Address part = 0; part < chunkCount && part < 256; ++part) {
    if (!ReadValue(chunks + part * 32, chunk) || chunk.count > 1024) break;
    for (std::uint64_t i = 0; i < chunk.count; ++i, ++index) {
      wire_group_draw::Command c{};
      if (!ReadValue(chunk.commands + i * 64, c) || c.bytes[0] == 7) continue;
      char line[400];
      int n = std::snprintf(line, sizeof(line), "%zu t%d %.2f %.2f %.2f %.2f %.2f %.2f |", index, c.bytes[0],
        wire_group_draw::FloatAt(c, 8), wire_group_draw::FloatAt(c, 12), wire_group_draw::FloatAt(c, 16),
        wire_group_draw::FloatAt(c, 20), wire_group_draw::FloatAt(c, 24), wire_group_draw::FloatAt(c, 28));
      for (int b = 1; b < 8; ++b) n += std::snprintf(line + n, sizeof(line) - n, "%02x", c.bytes[b]);
      n += std::snprintf(line + n, sizeof(line) - n, " ");
      for (int b = 32; b < 64; ++b) n += std::snprintf(line + n, sizeof(line) - n, "%02x", c.bytes[b]);
      text += line;
      if (c.bytes[0] == 8) { std::string label; if (ReadLabel(c, label)) text += " '" + label + "'"; }
      text += "\n";
      if (text.size() > std::size_t(size) - 512) { text += "...\n"; goto done; }
    }
  }
done:
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Graph node presenters under the model: class, accessor checks, and how many
// in/out port presenters each holds (ports nested in port groups included).
namespace {
int CountPorts(wire_group_native::Address p, int depth) {
  using namespace wire_group_native;
  int count = 0;
  std::unordered_set<Address> seen;
  for (; p && seen.insert(p).second && depth < 8; p = Word(p + 0xb8)) {
    const std::string name = ClassName(p);
    if (name == "net.maxon.mvp.presenterclasses.graphinport" || name == "net.maxon.mvp.presenterclasses.graphoutport") ++count;
    else count += CountPorts(Word(p + 0xd0), depth + 1);
  }
  return count;
}
}
extern "C" __declspec(dllexport) int probe_node_presenters(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address model = Word(Word(window + 0x548) + 0xd0);
  const Address module = reinterpret_cast<Address>(GetModuleHandleW(L"c4d_nodeeditor.xdl64"));
  const Address infoOffset = module ? Word(module + 0x28f750) : 0;
  std::string text;
  std::unordered_set<Address> seen;
  for (Address node = Word(model + 0xd0); node && seen.insert(node).second; node = Word(node + 0xb8)) {
    const Address ci = Word(node), size2 = Word(ci + 0x20);
    unsigned char valid = 0, code[5]{};
    Address fn = 0, delta = 0;
    if (size2 > infoOffset) { ReadValue(ci + infoOffset, valid); ReadValue(ci + infoOffset + 1, fn); ReadValue(ci + infoOffset + 9, delta); }
    const bool codeOk = fn && Read(fn, code, sizeof(code)) && std::memcmp(code, "\x48\x8d\x41\x08\xc3", 5) == 0;
    std::string lists;
    std::unordered_set<Address> seenList;
    for (Address l = Word(node + 0xd0); l && seenList.insert(l).second; l = Word(l + 0xb8))
      lists += " " + ClassName(l).substr(31) + ":" + std::to_string(CountPorts(Word(l + 0xd0), 0));
    char line[600];
    std::snprintf(line, sizeof(line), "%s valid=%d delta=%llu code=%d at180=%s |%s\n", ClassName(node).substr(31).c_str(), valid,
      (unsigned long long)delta, codeOk, ClassName(Word(node + 0x180)).c_str(), lists.c_str());
    text += line;
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Node bodies and port dots ParseNodes finds in a canvas's current drawing.
extern "C" __declspec(dllexport) int probe_nodes(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0) return 0;
  Drawing drawing;
  std::string text;
  if (!ReadDrawing(static_cast<HWND>(hwnd), drawing)) text = std::string("no drawing: ") + (drawing.failure ? drawing.failure : "?") + "\n";
  for (const auto& n : wire_group_draw::ParseNodes(drawing.commands, ReadLabel)) {
    char line[200];
    std::snprintf(line, sizeof(line), "body %.2f %.2f %.2f %.2f r=%.2f\n", n.x, n.y, n.w, n.h, n.radius);
    text += line;
    for (const auto* side : {&n.outs, &n.ins})
      for (const auto& c : *side) {
        std::snprintf(line, sizeof(line), "  %s %.2f ", side == &n.outs ? "out" : "in ", c.y);
        text += line + c.label + "\n";
      }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Drawlists reachable from the nodegraph window object within two pointer hops:
// offset path, command count and a histogram of command types.
extern "C" __declspec(dllexport) int probe_find_drawlists(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  std::string text;
  std::unordered_set<Address> seen;
  auto report = [&](Address list, const std::string& path) {
    if (!seen.insert(list).second || !ClassIs(list, "net.maxon.mvp.gluon2.drawlist")) return;
    const Address component = list + 0xe0, chunks = Word(component + 0x30), chunkCount = Word(component + 0x48);
    std::map<int, int> types;
    std::uint64_t total = 0;
    struct Chunk { Address unused, commands; std::uint64_t count, capacity; } chunk{};
    for (Address part = 0; part < chunkCount && part < 256; ++part) {
      if (!ReadValue(chunks + part * 32, chunk) || chunk.count > 1024) break;
      for (std::uint64_t i = 0; i < chunk.count; ++i) {
        wire_group_draw::Command c{};
        if (ReadValue(chunk.commands + i * 64, c)) { ++types[c.bytes[0]]; ++total; }
      }
    }
    text += path + " list=" + std::to_string(list) + " commands=" + std::to_string(total) + " types:";
    for (const auto& [t, n] : types) text += " t" + std::to_string(t) + "=" + std::to_string(n);
    text += "\n";
  };
  std::array<Address, 176> a{};
  for (int i = 0; i < 176; i += 44) if (!Read(window + i * 8, a.data() + i, 44 * 8)) return 0;
  for (int i = 0; i < 176; ++i) {
    if (a[i] < 0x10000000000ULL || a[i] >= 0x7f0000000000ULL || (a[i] & 7)) continue;
    char path[32]; std::snprintf(path, sizeof(path), "+0x%x", i * 8);
    report(a[i], path);
    std::array<Address, 64> b{};  // 512 bytes, the Read limit
    if (!Read(a[i], b.data(), sizeof(b))) continue;
    for (int j = 0; j < 64; ++j) {
      if (b[j] < 0x10000000000ULL || b[j] >= 0x7f0000000000ULL || (b[j] & 7)) continue;
      char path2[48]; std::snprintf(path2, sizeof(path2), "+0x%x+0x%x", i * 8, j * 8);
      report(b[j], path2);
    }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Class names of objects reachable from the nodegraph window (bounded BFS), with
// counts and the shallowest path, to locate how wires are presented or drawn.
// root: 0 = the nodegraph window, 1 = the graph model presenter.
extern "C" __declspec(dllexport) int probe_class_survey(void* hwnd, int maxDepth, char* out, int size, int root) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  struct Item { Address address; int depth; std::string path; };
  std::deque<Item> queue{{root == 1 ? Word(Word(window + 0x548) + 0xd0) : window, 0, ""}};
  std::unordered_set<Address> seen;
  std::map<std::string, std::pair<int, std::string>> classes;
  while (!queue.empty() && seen.size() < 40000) {
    Item item = queue.front(); queue.pop_front();
    if (!seen.insert(item.address).second) continue;
    const std::string name = ClassName(item.address);
    if (name.size() > 4 && name != "?" && name.find("net.maxon") == 0) {
      auto& entry = classes[name];
      if (entry.first++ == 0) entry.second = item.path;
    }
    if (item.depth >= maxDepth) continue;
    std::array<Address, 64> words{};
    if (!Read(item.address, words.data(), sizeof(words))) continue;
    for (int i = 0; i < 64; ++i) {
      const Address p = words[i];
      if (p >= 0x10000000000ULL && p < 0x7f0000000000ULL && !(p & 7) && !seen.count(p)) {
        char step[16]; std::snprintf(step, sizeof(step), "+%x", i * 8);
        queue.push_back({p, item.depth + 1, item.path + step});
      }
    }
  }
  std::string text;
  for (const auto& [name, entry] : classes) text += std::to_string(entry.first) + " " + name + " " + entry.second + "\n";
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Drawlists under the window's drawlist tree (+0x60), bounded BFS, with histograms.
extern "C" __declspec(dllexport) int probe_drawlist_tree(void* hwnd, int maxDepth, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  struct Item { Address address; int depth; std::string path; };
  std::deque<Item> queue{{Word(window + 0x60), 0, "tree"}};
  std::unordered_set<Address> seen;
  std::string text;
  while (!queue.empty() && seen.size() < 20000) {
    Item item = queue.front(); queue.pop_front();
    if (!seen.insert(item.address).second) continue;
    if (ClassIs(item.address, "net.maxon.mvp.gluon2.drawlist")) {
      const Address component = item.address + 0xe0, chunks = Word(component + 0x30), chunkCount = Word(component + 0x48);
      std::map<int, int> types;
      struct Chunk { Address unused, commands; std::uint64_t count, capacity; } chunk{};
      for (Address part = 0; part < chunkCount && part < 256; ++part) {
        if (!ReadValue(chunks + part * 32, chunk) || chunk.count > 1024) break;
        for (std::uint64_t i = 0; i < chunk.count; ++i) {
          wire_group_draw::Command c{};
          if (ReadValue(chunk.commands + i * 64, c)) ++types[c.bytes[0]];
        }
      }
      text += item.path + " list=" + std::to_string(item.address) + ":";
      for (const auto& [t, n] : types) text += " t" + std::to_string(t) + "=" + std::to_string(n);
      text += "\n";
    }
    if (item.depth >= maxDepth) continue;
    std::array<Address, 64> words{};
    if (!Read(item.address, words.data(), sizeof(words))) continue;
    for (int i = 0; i < 64; ++i) {
      const Address p = words[i];
      if (p >= 0x10000000000ULL && p < 0x7f0000000000ULL && !(p & 7) && !seen.count(p)) {
        char step[16]; std::snprintf(step, sizeof(step), "+%x", i * 8);
        queue.push_back({p, item.depth + 1, item.path + step});
      }
    }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Wire presenters: model+0xf0 and its +0xb8 siblings. For each, the first 0x300
// bytes as 8-byte words: class name for object pointers, else two floats.
extern "C" __declspec(dllexport) int probe_wires(void* hwnd, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address model = Word(Word(window + 0x548) + 0xd0);
  std::string text;
  std::unordered_set<Address> seen;
  int count = 0;
  for (Address w = Word(model + 0xf0); w && seen.insert(w).second && count < 40; w = Word(w + 0xb8), ++count) {
    text += "wire " + ClassName(w) + "\n";
    if (count >= 2) continue;
    for (int off = 0; off < 0x300; off += 8) {
      Address v = Word(w + off);
      float f[2]; std::memcpy(f, &v, 8);
      char line[200];
      const std::string cls = (v >= 0x10000000000ULL && v < 0x7f0000000000ULL && !(v & 7)) ? ClassName(v) : "";
      std::snprintf(line, sizeof(line), "  +%03x %016llx %s %g %g\n", off, (unsigned long long)v,
        cls.size() > 1 ? cls.c_str() : "", f[0], f[1]);
      text += line;
    }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Raw memory at the wire presenter's field `field` (a pointer): `bytes` bytes as floats.
extern "C" __declspec(dllexport) int probe_wire_field(void* hwnd, int field, int bytes, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address wire = Word(Word(Word(window + 0x548) + 0xd0) + 0xf0);
  const Address data = Word(wire + field);
  std::string text;
  for (int off = 0; off < bytes; off += 32) {
    std::array<float, 8> f{};
    if (!Read(data + off, f.data(), sizeof(f))) break;
    char line[256];
    std::snprintf(line, sizeof(line), "+%04x %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f\n", off, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
    text += line;
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Object at element `index` of the pointer array in wire presenter field `field`:
// class and its first `bytes` bytes as words (class name or two floats).
extern "C" __declspec(dllexport) int probe_wire_item(void* hwnd, int field, int index, int bytes, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address wire = Word(Word(Word(window + 0x548) + 0xd0) + 0xf0);
  const Address item = Word(Word(wire + field) + index * 8);
  std::string text = "item class " + ClassName(item) + "\n";
  for (int off = 0; off < bytes; off += 8) {
    const Address v = Word(item + off);
    float f[2]; std::memcpy(f, &v, 8);
    double d; std::memcpy(&d, &v, 8);
    const std::string cls = (v >= 0x10000000000ULL && v < 0x7f0000000000ULL && !(v & 7)) ? ClassName(v) : "";
    char line[220];
    std::snprintf(line, sizeof(line), "  +%03x %016llx %s f=%g,%g d=%g\n", off, (unsigned long long)v, cls.size() > 1 ? cls.c_str() : "", f[0], f[1], d);
    text += line;
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// The drawlist tree's first list (+0x10): chunk header and stale commands up to capacity.
extern "C" __declspec(dllexport) int probe_tree_list(void* hwnd, int which, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address list = Word(Word(window + 0x60) + which), component = list + 0xe0;
  const Address chunks = Word(component + 0x30), chunkCount = Word(component + 0x48);
  std::string text = "class " + ClassName(list) + " chunks=" + std::to_string(chunkCount) + " flag=" + std::to_string(Word(component + 0x58)) + "\n";
  struct Chunk { Address unused, commands; std::uint64_t count, capacity; } chunk{};
  for (Address part = 0; part < 4 && chunks; ++part) {
    if (!ReadValue(chunks + part * 32, chunk)) break;
    text += "chunk " + std::to_string(part) + " count=" + std::to_string(chunk.count) + " capacity=" + std::to_string(chunk.capacity) + "\n";
    std::map<int, int> types;
    for (std::uint64_t i = 0; i < chunk.capacity && i < 1024; ++i) {
      wire_group_draw::Command c{};
      if (!ReadValue(chunk.commands + i * 64, c)) break;
      ++types[c.bytes[0]];
      if (i < 12) {
        char line[200];
        std::snprintf(line, sizeof(line), "  %llu t%d %.2f %.2f %.2f %.2f %.2f %.2f\n", (unsigned long long)i, c.bytes[0],
          wire_group_draw::FloatAt(c, 8), wire_group_draw::FloatAt(c, 12), wire_group_draw::FloatAt(c, 16),
          wire_group_draw::FloatAt(c, 20), wire_group_draw::FloatAt(c, 24), wire_group_draw::FloatAt(c, 28));
        text += line;
      }
    }
    text += "  types:";
    for (const auto& [t, n] : types) text += " t" + std::to_string(t) + "=" + std::to_string(n);
    text += "\n";
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// For every graph node presenter under the model: offsets (in the presenter and in
// objects it points to, one hop) where a float pair equals one of `targets`
// (x0,y0,x1,y1,...), to locate where the editor keeps a node's position.
extern "C" __declspec(dllexport) int probe_presenter_floats(void* hwnd, const float* targets, int count, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address model = Word(Word(window + 0x548) + 0xd0);
  std::string text;
  auto scan = [&](Address base, int bytes, const std::string& where) {
    for (int off = 0; off + 8 <= bytes; off += 4) {
      float f[2];
      if (!Read(base + off, f, 8)) break;
      double d[2] = {0, 0};
      const bool doubles = (off % 8 == 0) && off + 16 <= bytes && Read(base + off, d, 16);
      for (int t = 0; t < count; ++t) {
        if (std::fabs(f[0] - targets[2 * t]) < 0.6f && std::fabs(f[1] - targets[2 * t + 1]) < 0.6f) {
          char line[160];
          std::snprintf(line, sizeof(line), "%s +0x%x float = target %d (%.2f, %.2f)\n", where.c_str(), off, t, f[0], f[1]);
          text += line;
        }
        if (doubles && std::fabs(d[0] - targets[2 * t]) < 0.6 && std::fabs(d[1] - targets[2 * t + 1]) < 0.6) {
          char line[160];
          std::snprintf(line, sizeof(line), "%s +0x%x double = target %d (%.2f, %.2f)\n", where.c_str(), off, t, d[0], d[1]);
          text += line;
        }
      }
    }
  };
  std::unordered_set<Address> seen;
  int index = 0;
  for (Address node = Word(model + 0xd0); node && seen.insert(node).second; node = Word(node + 0xb8), ++index) {
    const std::string cls = ClassName(node);
    if (cls.find("graphnode") == std::string::npos) continue;
    const std::string name = "presenter " + std::to_string(index);
    for (int chunk = 0; chunk < 0x600; chunk += 0x200) scan(node + chunk, 0x200, name + " base" + std::to_string(chunk));
    for (int off = 0; off < 0x600; off += 8) {
      const Address p = Word(node + off);
      if (p < 0x10000000000ULL || p >= 0x7f0000000000ULL || (p & 7)) continue;
      char where[64]; std::snprintf(where, sizeof(where), "%s ->+0x%x", name.c_str(), off);
      scan(p, 0x200, where);
    }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Every committed read/write private memory region: addresses where a float pair
// equals one of `targets` (tolerance 0.05). Up to `maxHits` lines.
extern "C" __declspec(dllexport) int probe_scan_memory(const float* targets, int count, int maxHits, char* out, int size) {
  if (!out || size <= 0) return 0;
  std::string text;
  int hits = 0;
  MEMORY_BASIC_INFORMATION mbi{};
  std::vector<unsigned char> buffer;
  for (unsigned char* p = nullptr; VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi); p = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize) {
    if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || !(mbi.Protect & (PAGE_READWRITE)) || (mbi.Protect & PAGE_GUARD)) continue;
    if (mbi.RegionSize > (512u << 20)) continue;
    buffer.resize(mbi.RegionSize);
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), mbi.BaseAddress, buffer.data(), mbi.RegionSize, &got)) continue;
    const float* f = reinterpret_cast<const float*>(buffer.data());
    const std::size_t n = got / 4;
    for (std::size_t i = 0; i + 1 < n; ++i) {
      for (int t = 0; t < count; ++t) {
        if (std::fabs(f[i] - targets[2 * t]) < 0.05f && std::fabs(f[i + 1] - targets[2 * t + 1]) < 0.05f) {
          char line[120];
          std::snprintf(line, sizeof(line), "%p target %d\n", static_cast<void*>(static_cast<unsigned char*>(mbi.BaseAddress) + i * 4), t);
          text += line;
          if (++hits >= maxHits) goto done;
        }
      }
    }
  }
done:
  const std::size_t len = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), len); out[len] = 0;
  return hits;
}
// Class name of the object whose header is at or below `address` (walks back in
// 8-byte steps up to `back` bytes looking for a valid class info pointer).
extern "C" __declspec(dllexport) int probe_owner(void* address, int back, char* out, int size) {
  using namespace wire_group_native;
  std::string text;
  const Address a = reinterpret_cast<Address>(address) & ~Address(7);
  for (int off = 0; off <= back; off += 8) {
    const std::string name = ClassName(a - off);
    if (name.size() > 6 && name != "?" && name.find("net.maxon") == 0) {
      text = name + " at -0x" + [&] { char b[16]; std::snprintf(b, sizeof(b), "%x", off); return std::string(b); }();
      break;
    }
  }
  const std::size_t len = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), len); out[len] = 0;
  return int(len);
}
// Like probe_scan_memory, for pairs of doubles (8-byte aligned).
extern "C" __declspec(dllexport) int probe_scan_doubles(const double* targets, int count, int maxHits, char* out, int size) {
  if (!out || size <= 0) return 0;
  std::string text;
  int hits = 0;
  MEMORY_BASIC_INFORMATION mbi{};
  std::vector<unsigned char> buffer;
  for (unsigned char* p = nullptr; VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi); p = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize) {
    if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || !(mbi.Protect & PAGE_READWRITE) || (mbi.Protect & PAGE_GUARD)) continue;
    if (mbi.RegionSize > (512u << 20)) continue;
    buffer.resize(mbi.RegionSize);
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), mbi.BaseAddress, buffer.data(), mbi.RegionSize, &got)) continue;
    const double* d = reinterpret_cast<const double*>(buffer.data());
    const std::size_t n = got / 8;
    for (std::size_t i = 0; i + 1 < n; ++i)
      for (int t = 0; t < count; ++t)
        if (std::fabs(d[i] - targets[2 * t]) < 0.01 && std::fabs(d[i + 1] - targets[2 * t + 1]) < 0.01) {
          char line[120];
          std::snprintf(line, sizeof(line), "%p target %d\n", static_cast<void*>(static_cast<unsigned char*>(mbi.BaseAddress) + i * 8), t);
          text += line;
          if (++hits >= maxHits) goto done;
        }
  }
done:
  const std::size_t len = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), len); out[len] = 0;
  return hits;
}
// From every graph node presenter, bounded BFS over pointer words (64 per object,
// depth <= maxDepth): paths reaching an address in [target - before, target].
extern "C" __declspec(dllexport) int probe_path_to(void* hwnd, unsigned long long target, int before, int maxDepth, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  const Address model = Word(Word(window + 0x548) + 0xd0);
  std::string text;
  std::unordered_set<Address> presenters;
  int index = 0;
  for (Address node = Word(model + 0xd0); node && presenters.insert(node).second; node = Word(node + 0xb8), ++index) {
    if (ClassName(node).find("graphnode") == std::string::npos) continue;
    struct Item { Address a; int depth; std::string path; };
    std::deque<Item> queue{{node, 0, "p" + std::to_string(index)}};
    std::unordered_set<Address> seen;
    while (!queue.empty() && seen.size() < 20000) {
      Item it = queue.front(); queue.pop_front();
      if (!seen.insert(it.a).second) continue;
      if (it.a <= target && target - it.a <= Address(before)) {
        char line[300];
        std::snprintf(line, sizeof(line), "%s -> %p (target-0x%llx) class=%s\n", it.path.c_str(), (void*)it.a,
          (unsigned long long)(target - it.a), ClassName(it.a).c_str());
        text += line;
      }
      if (it.depth >= maxDepth) continue;
      std::array<Address, 64> words{};
      if (!Read(it.a, words.data(), sizeof(words))) continue;
      for (int i = 0; i < 64; ++i) {
        const Address p = words[i];
        if (p >= 0x10000000000ULL && p < 0x7f0000000000ULL && !(p & 7) && !seen.count(p)) {
          char step[16]; std::snprintf(step, sizeof(step), "+%x", i * 8);
          queue.push_back({p, it.depth + 1, it.path + step});
        }
      }
    }
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Like probe_path_to, starting at the nodegraph window object.
extern "C" __declspec(dllexport) int probe_path_from_window(void* hwnd, unsigned long long target, int before, int maxDepth, int maxNodes, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  std::string text;
  struct Item { Address a; int depth; std::string path; };
  std::deque<Item> queue{{window, 0, "w"}};
  std::unordered_set<Address> seen;
  while (!queue.empty() && int(seen.size()) < maxNodes) {
    Item it = queue.front(); queue.pop_front();
    if (!seen.insert(it.a).second) continue;
    if (it.a <= target && target - it.a <= Address(before)) {
      char line[400];
      std::snprintf(line, sizeof(line), "%s -> %p (target-0x%llx) class=%s\n", it.path.c_str(), (void*)it.a,
        (unsigned long long)(target - it.a), ClassName(it.a).c_str());
      text += line;
      if (text.size() > 20000) break;
    }
    if (it.depth >= maxDepth) continue;
    std::array<Address, 64> words{};
    if (!Read(it.a, words.data(), sizeof(words))) continue;
    for (int i = 0; i < 64; ++i) {
      const Address p = words[i];
      if (p >= 0x10000000000ULL && p < 0x7f0000000000ULL && !(p & 7) && !seen.count(p)) {
        char step[16]; std::snprintf(step, sizeof(step), "+%x", i * 8);
        queue.push_back({p, it.depth + 1, it.path + step});
      }
    }
  }
  char tail[64]; std::snprintf(tail, sizeof(tail), "visited %zu\n", seen.size());
  text += tail;
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Follows `steps` byte offsets from the nodegraph window, printing each object's
// class and, for the last one, its first 64 words with their classes.
extern "C" __declspec(dllexport) int probe_follow(void* hwnd, const int* steps, int count, char* out, int size) {
  using namespace wire_group_native;
  if (!out || size <= 0 || !SupportedBuild()) return 0;
  const Address os = WindowObject(static_cast<HWND>(hwnd)), window = os ? FindWindow(os) : 0;
  if (!window) return 0;
  std::string text;
  Address a = window;
  for (int i = 0; i < count; ++i) {
    char line[200];
    std::snprintf(line, sizeof(line), "%p %s\n", (void*)a, ClassName(a).c_str());
    text += line;
    a = Word(a + steps[i]);
  }
  char line[200];
  std::snprintf(line, sizeof(line), "end %p %s\n", (void*)a, ClassName(a).c_str());
  text += line;
  for (int i = 0; i < 64; ++i) {
    const Address v = Word(a + i * 8);
    const std::string cls = (v >= 0x10000000000ULL && v < 0x7f0000000000ULL && !(v & 7)) ? ClassName(v) : "";
    std::snprintf(line, sizeof(line), "  +%03x %016llx %s\n", i * 8, (unsigned long long)v, cls.size() > 1 ? cls.c_str() : "");
    text += line;
  }
  const std::size_t n = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), n); out[n] = 0;
  return 1;
}
// Every 8-byte aligned location in private read/write memory that holds a pointer
// into [target - before, target].
extern "C" __declspec(dllexport) int probe_scan_pointers(unsigned long long target, int before, int maxHits, char* out, int size) {
  if (!out || size <= 0) return 0;
  std::string text;
  int hits = 0;
  MEMORY_BASIC_INFORMATION mbi{};
  std::vector<unsigned char> buffer;
  for (unsigned char* p = nullptr; VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi); p = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize) {
    if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || !(mbi.Protect & PAGE_READWRITE) || (mbi.Protect & PAGE_GUARD)) continue;
    if (mbi.RegionSize > (512u << 20)) continue;
    buffer.resize(mbi.RegionSize);
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), mbi.BaseAddress, buffer.data(), mbi.RegionSize, &got)) continue;
    const unsigned long long* q = reinterpret_cast<const unsigned long long*>(buffer.data());
    for (std::size_t i = 0; i < got / 8; ++i)
      if (q[i] <= target && target - q[i] <= (unsigned long long)before) {
        char line[120];
        std::snprintf(line, sizeof(line), "%p -> %llx\n", static_cast<void*>(static_cast<unsigned char*>(mbi.BaseAddress) + i * 8), q[i]);
        text += line;
        if (++hits >= maxHits) goto done;
      }
  }
done:
  const std::size_t len = (std::min)(text.size(), std::size_t(size - 1));
  std::memcpy(out, text.data(), len); out[len] = 0;
  return hits;
}
