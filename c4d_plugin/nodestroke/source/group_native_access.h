#pragma once

// Read-only C4D 2027.0 Windows research adapter. All offsets are build-specific.
// Every failure leaves group interface operations disabled for that gesture.
#include "group_draw_data.h"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <deque>
#include <unordered_set>
#pragma comment(lib, "bcrypt.lib")

namespace wire_group_native {
using Address = std::uintptr_t;
inline bool Read(Address p, void* dst, std::size_t n) {
  SIZE_T got = 0;
  return p && n && n <= 512 && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(p), dst, n, &got) && got == n;
}
template <typename T> inline bool ReadValue(Address p, T& value) { return Read(p, &value, sizeof(value)); }
inline Address Word(Address p) { Address v = 0; ReadValue(p, v); return v; }

inline bool ClassIs(Address object, const char* expected) {
  const Address info = Word(object);
  struct Header { Address a, b, name; std::uint64_t size; } header{};
  if (!ReadValue(info, header) || header.size != std::strlen(expected) || header.size > 160) return false;
  std::array<char, 161> name{};
  return Read(header.name, name.data(), static_cast<std::size_t>(header.size)) &&
         std::memcmp(name.data(), expected, static_cast<std::size_t>(header.size)) == 0;
}

inline bool ModuleHash(const wchar_t* name, const char* expected) {
  HMODULE module = GetModuleHandleW(name);
  wchar_t path[32768]{};
  if (!module || !GetModuleFileNameW(module, path, DWORD(std::size(path)))) return false;
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
  bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
            BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
  std::array<unsigned char, 65536> data{}; DWORD count = 0;
  while (ok) {
    if (!ReadFile(file, data.data(), DWORD(data.size()), &count, nullptr)) { ok = false; break; }
    if (!count) break;
    ok = BCryptHashData(hash, data.data(), count, 0) >= 0;
  }
  std::array<unsigned char, 32> digest{};
  if (ok) ok = BCryptFinishHash(hash, digest.data(), DWORD(digest.size()), 0) >= 0;
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  CloseHandle(file);
  const char hex[] = "0123456789abcdef";
  if (!ok || std::strlen(expected) != 64) return false;
  for (std::size_t i = 0; i < digest.size(); ++i)
    if (expected[i * 2] != hex[digest[i] >> 4] || expected[i * 2 + 1] != hex[digest[i] & 15]) return false;
  return true;
}
inline bool SupportedBuild() {
  static const bool supported = ModuleHash(L"gui.module.xdl64", "2274410ec73d65612b39c29f50f37dc870e6d5aa8649c214159606e145d82217") &&
    ModuleHash(L"c4d_nodeeditor.xdl64", "4528e0fd90c841cbb79014c1af4773380aebb59afdbda988b950804a242ff4cf") &&
    ModuleHash(L"gui_gluon.module.xdl64", "11b4611c4b589f7bd08d36575d24cb825c2e376049bb9aee141c6a2344427122") &&
    ModuleHash(L"presenter_nodes.module.xdl64", "060432a909a7d418b7abe940e75ccc8817141257e8fb77eb3cef006bd8508279");
  return supported;
}

inline Address WindowObject(HWND hwnd) {
  const auto base = reinterpret_cast<Address>(GetModuleHandleW(L"gui.module.xdl64"));
  const auto proc = static_cast<Address>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
  if (!base || proc != base + 0x23190) return 0;
  const Address table = Word(base + 0x12de60), mask = Word(base + 0x12de68);
  if (!table || mask > 1000000) return 0;
  std::uint64_t value = reinterpret_cast<Address>(hwnd);
  value ^= value >> 32; value ^= (value >> 12) ^ (value >> 20); value ^= (value >> 4) ^ (value >> 7);
  Address node = Word(table + 16 * (mask & value)), match = 0;
  for (int i = 0; node && i < 64; ++i) {
    if (Word(node + 8) == value) {
      const Address weak = Word(node + 24), object = Word(weak);
      if (object && !(object & 1) && Word(node + 24) == weak && Word(weak) == object &&
          ClassIs(object, "net.maxon.ui.oswindowclass.widgetuserdraw")) {
        if (match) return 0;
        match = object;
      }
    }
    node = Word(node);
  }
  return node ? 0 : match;
}

// Follow bounded candidate words, then verify a window's explicit OS-window
// link. The search path itself is never trusted as an ownership relation.
inline Address FindWindow(Address osWindow) {
  static Address cachedOs = 0, cachedWindow = 0;
  if (cachedOs == osWindow && ClassIs(cachedWindow, "net.maxon.mvp.gluon2.windowclasses.nodegraph") &&
      Word(cachedWindow + 0x50) == osWindow) return cachedWindow;
  cachedOs = cachedWindow = 0;
  struct Candidate { Address address; unsigned depth; };
  std::deque<Candidate> queue{{osWindow, 0}};
  std::unordered_set<Address> seen;
  while (!queue.empty() && seen.size() < 60000) {
    const auto item = queue.front(); queue.pop_front();
    if (!seen.insert(item.address).second) continue;
    if (ClassIs(item.address, "net.maxon.mvp.gluon2.windowclasses.nodegraph") && Word(item.address + 0x50) == osWindow) {
      // The matching nodegraph window explicitly points back to this HWND's
      // native OS window; do not rely on the search path as an association.
      cachedOs = osWindow;
      cachedWindow = item.address;
      return item.address;
    }
    if (item.depth >= 5) continue;
    std::array<Address, 64> words{};
    if (!Read(item.address, words.data(), sizeof(words))) continue;
    for (const Address p : words)
      if (p >= 0x10000000000ULL && p < 0x7f0000000000ULL && !(p & 7) && !seen.count(p))
        queue.push_back({p, item.depth + 1});
    if (queue.size() > 600000) return 0;
  }
  return 0;
}

inline bool ReadLabel(const wire_group_draw::Command& command, std::string& result) {
  Address text = 0; std::memcpy(&text, command.bytes + 48, sizeof(text));
  std::array<unsigned char, 48> header{};
  if (!Read(text, header.data(), header.size())) return false;
  Address payload = 0; std::uint64_t length = 0;
  std::memcpy(&payload, header.data() + 24, 8); std::memcpy(&length, header.data() + 32, 8);
  if (!length || length > 256) return false;
  std::array<char, 256> bytes{};
  if (!Read(payload, bytes.data(), static_cast<std::size_t>(length))) return false;
  if (header[0] == 0xfe || header[0] == 0xff) {
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), int(length), nullptr, 0) <= 0) return false;
    result.assign(bytes.data(), static_cast<std::size_t>(length)); return true;
  }
  if (header[0] != 0 || length % 2) return false;
  std::array<wchar_t, 128> wide{}; std::memcpy(wide.data(), bytes.data(), static_cast<std::size_t>(length));
  const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), int(length / 2), nullptr, 0, nullptr, nullptr);
  if (count <= 0 || count > 1024) return false;
  result.resize(static_cast<std::size_t>(count));
  return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), int(length / 2), result.data(), count, nullptr, nullptr) == count;
}

// A consistent copy of a canvas's drawlist and the presenter of the graph it shows.
struct Drawing { Address window = 0, model = 0; std::vector<wire_group_draw::Command> commands; const char* failure = nullptr; };
inline bool ReadDrawing(HWND hwnd, Drawing& result) {
  result = {};
  auto fail = [&](const char* reason) { result.failure = reason; return false; };
  if (!SupportedBuild()) return fail("unsupported build");
  const Address os = WindowObject(hwnd), window = os ? FindWindow(os) : 0;
  if (!window) return fail("native window not found");
  const Address list = Word(window + 0x58), app = Word(window + 0x548);
  if (!ClassIs(list, "net.maxon.mvp.gluon2.drawlist") || !ClassIs(app, "net.maxon.mvp.presenterclasses.app")) return fail("drawlist or app type");
  const Address model = Word(app + 0xd0);
  if (!ClassIs(model, "net.maxon.mvp.presenterclasses.graphmodel")) return fail("graph model type");
  const Address component = list + 0xe0, chunks = Word(component + 0x30);
  const Address chunkCount = Word(component + 0x48);
  if (!chunkCount || chunkCount > 256 || Word(component + 0x58) != 1) return fail("drawlist chunk header");
  struct Chunk { Address unused, commands; std::uint64_t count, capacity; } chunk{};
  for (Address part = 0; part < chunkCount; ++part) {
    if (!ReadValue(chunks + part * 32, chunk) || chunk.count > chunk.capacity || chunk.capacity > 1024 ||
        (part + 1 < chunkCount && chunk.count != 1024)) return fail("drawlist chunk layout");
    for (std::uint64_t i = 0; i < chunk.count; ++i) {
      wire_group_draw::Command command{};
      if (!ReadValue(chunk.commands + i * 64, command)) return fail("drawlist read");
      result.commands.push_back(command);
    }
  }
  if (Word(component + 0x30) != chunks || Word(component + 0x48) != chunkCount ||
      Word(window + 0x58) != list || Word(app + 0xd0) != model) return fail("drawlist changed while reading");
  result.window = window; result.model = model; return true;
}

struct Snapshot { Address window = 0, model = 0; wire_group_draw::Panels panels; const char* failure = nullptr; };
// Side panels of a group view, read from a drawing.
inline bool Capture(const Drawing& drawing, Snapshot& result) {
  result = {};
  if (!drawing.window) { result.failure = drawing.failure ? drawing.failure : "no drawing"; return false; }
  if (!wire_group_draw::Parse(drawing.commands, ReadLabel, result.panels, &result.failure)) return false;
  result.window = drawing.window; result.model = drawing.model; return true;
}
inline bool Capture(HWND hwnd, Snapshot& result) {
  Drawing drawing;
  ReadDrawing(hwnd, drawing);
  return Capture(drawing, result);
}
}
