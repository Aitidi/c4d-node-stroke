// Mouse-stroke gestures on the native node editor canvas (Windows only).
// Hold I and move the mouse across nodes to connect them in stroke order; hold Y
// and move across wires to cut them. Releasing the key applies the stroke.
// While dragging a node normally: shake it left/right to take it out of its wires, or drag an
// unconnected node onto a wire and rest briefly to splice it in. The canvas has no public API: screen positions are
// derived from the undocumented 2027 view/node layout records (native_layout.h)
// and the node editor's own style values (see kHeader below).
#include "c4d.h"
#include "graph_ops.h"
#include "rs_rules.h"
#include "native_layout.h"
#include "view_binding.h"
#include "wire_prefs.h"
#include "maxon/derived_portattributes.h"
#include "maxon/derived_nodeattributes.h"
#include "maxon/datadescription_ui.h"
#include "maxon/datadescription_data.h"
#include "maxon/stringresource.h"
#include "maxon/datadescriptiondatabase.h"
#include "maxon/assets.h"
#include <atomic>

using namespace cinema;
cinema::Bool RegisterCanvasGestures();
void StartCanvasGestures();
void StopCanvasGestures();

#if defined(MAXON_TARGET_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <algorithm>
#include <vector>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "group_native_access.h"
#include "../res/c4d_symbols.h"
#undef GetMessage

namespace {
// Local development IDs, not registered for distribution.
constexpr Int32 kGestureMessageId = 1069987;
constexpr Int32 kToggleId = 1069986;

// Node layout in graph units (zoom 1). Row sizes come from the editor stylesheet
// (<prefs>/gluon/nodeeditor_stylesheet.json: portRowHeight 20, categoryHeaderHeight
// 20, paddingBetweeIconBarAndPortGroup 4, extraSpaceAtBottomOfNode 12). Measured
// against the editor's own click selection: the layout
// record's y is where the port list starts; title, icon bar and preview sit above it.
// Shown ports are those without hideportinnodegraph plus connected ones; outputs
// come first, then inputs in UI-description order, with a title row before the
// first shown port of its own (innermost) group when that group is titled.
constexpr Float kHeader = 4.0;     // record y -> top of the first port row
// Extent above the record y: title row only, or title plus the preview image that
// material and output nodes show (measured: their title sits ~108 above the ports).
constexpr Float kAboveTitle = 28.0;
constexpr Float kAbovePreview = 120.0;
constexpr Float kRow = 20.0;
constexpr Float kGroupHeader = 20.0;
constexpr Float kPadding = 12.0;
constexpr Float kTitleRow = 28.0;  // topOfNodePadding 8 + titleHeight 20 above the ports
// Port dots sit on the node edges, so hits extend this far outside the node box.
constexpr Float kEdgeSlack = 8.0;
// Screen pixels a cut stroke must reach past a wire on both sides to cut it.
constexpr Float kCutMargin = 6.0;

// Measured canvas-client to view-origin offset in screen pixels; (0,0) until calibrated.
constexpr Int32 kOriginDx = 0;
constexpr Int32 kOriginDy = 0;

enum class Mode { NONE, CONNECT, CUT, SHAKE };
// Shake: this many direction reversals, each moving at least kShakePixels.
constexpr Int32 kShakePixels = 24;
constexpr Int kShakeReversals = 4;
// Drop-insert: when a dragged, unwired node rests this long across a wire, it is
// inserted at once, while still being dragged.
constexpr ULONGLONG kDwellMs = 300;
// The editor's drag loop only pumps mouse messages, so a watcher thread posts a
// WM_MOUSEMOVE carrying this flag to wake the hook; the hook swallows it.
constexpr WPARAM kDwellMarker = 0x100000;
struct ScreenPoint { Int32 x, y; };
struct ViewSnapshot { maxon::GraphNode container; wire_native::ViewRecord view; };
struct Binding { HWND hwnd; maxon::GraphNode root, container; maxon::Id key; };

struct State {
  HHOOK hook = nullptr;
  // Copies of the preference switches (wire_prefs.h), refreshed by ApplyGesturePrefs.
  Bool enabled = true, connect = true, cut = true, shake = true, drop = true;
  Mode mode = Mode::NONE;
  WPARAM key = 0;
  UINT_PTR pollTimer = 0;
  HWND canvas = nullptr;
  HWND overlay = nullptr;
  RECT overlayRect = {};
  maxon::BaseArray<ScreenPoint> stroke;
  Mode pendingMode = Mode::NONE;
  HWND pendingCanvas = nullptr;
  maxon::BaseArray<ScreenPoint> pending;
  maxon::BaseArray<Binding> bindings;
  maxon::GraphNode contextRoot;
  maxon::GraphNode snapshotRoot;
  maxon::GraphNode strokeRoot;
  maxon::GraphNode pendingRoot;
  maxon::GraphNode dragRoot;
  // Native left-drag on a canvas, observed (never consumed) for shake and drop.
  Bool dragging = false;
  Bool shaken = false;
  ScreenPoint dragStart = {};
  Int32 shakePivot = 0, shakeExtreme = 0, shakeDirection = 0;
  Int shakeReversals = 0;
  HWND dragCanvas = nullptr;
  ScreenPoint lastMove = {};
  // Node under the cursor at button-down and its layout position then; while dragged
  // it follows the cursor (the layout record is not updated until the drop).
  maxon::GraphNode grabNode;
  Float grabX = 0, grabY = 0;
  Bool inserted = false;
  std::atomic<Bool> watchDrag{false};
  std::atomic<HWND> watchCanvas{nullptr};
  std::atomic<Bool> stopWatcher{false};
  HANDLE watcher = nullptr;
  HANDLE dragEvent = nullptr;  // set while a canvas drag runs; the watcher sleeps on it
  maxon::BaseArray<ViewSnapshot> snapshot;
  HWND trackedCanvas = nullptr;
};
State g;

Bool IsCanvasCandidate(HWND hwnd) {
  char name[64] = {};
  DWORD process = 0;
  if (!hwnd || !IsWindowVisible(hwnd) || !GetWindowThreadProcessId(hwnd, &process) || process != GetCurrentProcessId()) return false;
  const HWND root = GetAncestor(hwnd, GA_ROOT);
  const HWND foreground = GetForegroundWindow();
  return root && foreground && root == GetAncestor(foreground, GA_ROOT) &&
    GetClassNameA(hwnd, name, sizeof(name)) && strcmp(name, "Static") == 0;
}
HWND CanvasUnderCursor() {
  POINT cursor;
  if (!GetCursorPos(&cursor)) return nullptr;
  HWND under = WindowFromPoint(cursor);
  return IsCanvasCandidate(under) ? under : nullptr;
}

// View transforms live on each graph container (the material root or a nested
// group). Never combine child nodes from different containers into one layout.
maxon::Result<void> CollectViews(const maxon::GraphNode& container, maxon::BaseArray<ViewSnapshot>& out, Int depth) {
  iferr_scope;
  if (depth > 64) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_GESTURE_GROUP_DEPTH));
  iferr (const maxon::BaseArray<wire_native::ViewRecord> records = wire_native::ReadViews(container)) {}
  else {
    for (const auto& record : records) { out.Append(ViewSnapshot{container, record}) iferr_return; }
  }
  container.GetChildren([&](const maxon::GraphNode& child) -> maxon::Result<Bool> {
    iferr_scope;
    CollectViews(child, out, depth + 1) iferr_return;
    return true;
  }, maxon::NODE_KIND::NODE) iferr_return;
  return maxon::OK;
}
maxon::Result<maxon::BaseArray<ViewSnapshot>> ReadActiveViews() {
  iferr_scope;
  const maxon::nodes::NodesGraphModelRef graph = wire_ops::ActiveGraph(GetActiveDocument()) iferr_return;
  maxon::BaseArray<ViewSnapshot> out;
  CollectViews(graph.GetViewRoot(), out, 0) iferr_return;
  return out;
}
// A binding is only evidence for the graph whose view actually changed during
// calibration. Never transfer it to another active material/document.
maxon::Result<maxon::GraphNode> CurrentRoot() {
  iferr_scope;
  const maxon::nodes::NodesGraphModelRef graph = wire_ops::ActiveGraph(GetActiveDocument()) iferr_return;
  return graph.GetViewRoot();
}
Bool RefreshContext() {
  iferr (const maxon::GraphNode root = CurrentRoot()) {
    g.bindings.Flush(); g.contextRoot = maxon::GraphNode();
    return false;
  }
  if (!g.contextRoot.IsValid() || !(g.contextRoot == root)) {
    g.bindings.Flush();
    g.contextRoot = root;
  }
  return true;
}
// A node editor canvas of the supported build: its view is found from its drawing
// (ResolveView), so it needs no scroll binding. Misses are remembered for a moment
// because the search behind a miss is the expensive case.
Bool NativeCanvas(HWND canvas) {
  static HWND missed = nullptr;
  static ULONGLONG missedAt = 0;
  if (canvas == missed && GetTickCount64() - missedAt < 3000) return false;
  using namespace wire_group_native;
  const Address os = SupportedBuild() ? WindowObject(canvas) : 0;
  if (os && FindWindow(os)) return true;
  missed = canvas;
  missedAt = GetTickCount64();
  return false;
}
Bool HasBoundCanvas(HWND canvas) {
  if (!IsCanvasCandidate(canvas) || !RefreshContext()) return false;
  for (const Binding& b : g.bindings)
    if (b.hwnd == canvas && b.root == g.contextRoot) return true;
  return NativeCanvas(canvas);
}

// --- stroke overlay -------------------------------------------------------
// The overlay is a per-pixel-alpha layered window drawn with GDI+ (anti-aliased, round
// caps). The stroke is smoothed for display only; hit tests use the raw samples.
struct OverlaySurface {
  HDC dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ old = nullptr;
  Int32 width = 0, height = 0;
};
OverlaySurface g_surface;
ULONG_PTR g_gdiplus = 0;

void ReleaseSurface() {
  if (g_surface.dc) {
    SelectObject(g_surface.dc, g_surface.old);
    DeleteObject(g_surface.bitmap);
    DeleteDC(g_surface.dc);
  }
  g_surface = OverlaySurface();
}
Bool EnsureSurface(Int32 width, Int32 height) {
  if (g_surface.dc && g_surface.width == width && g_surface.height == height) return true;
  ReleaseSurface();
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  g_surface.dc = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  g_surface.bitmap = CreateDIBSection(g_surface.dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!g_surface.dc || !g_surface.bitmap) { ReleaseSurface(); return false; }
  g_surface.old = SelectObject(g_surface.dc, g_surface.bitmap);
  g_surface.width = width;
  g_surface.height = height;
  return true;
}
// Chaikin corner cutting, twice: rounds the polyline without moving it noticeably.
void SmoothStroke(const maxon::BaseArray<ScreenPoint>& in, std::vector<Gdiplus::PointF>& out) {
  out.clear();
  for (const ScreenPoint& p : in) out.push_back(Gdiplus::PointF(Gdiplus::REAL(p.x - g.overlayRect.left), Gdiplus::REAL(p.y - g.overlayRect.top)));
  for (int pass = 0; pass < 2 && out.size() > 2; ++pass) {
    std::vector<Gdiplus::PointF> next;
    next.push_back(out.front());
    for (size_t i = 0; i + 1 < out.size(); ++i) {
      const Gdiplus::PointF a = out[i], b = out[i + 1];
      next.push_back(Gdiplus::PointF(0.75f * a.X + 0.25f * b.X, 0.75f * a.Y + 0.25f * b.Y));
      next.push_back(Gdiplus::PointF(0.25f * a.X + 0.75f * b.X, 0.25f * a.Y + 0.75f * b.Y));
    }
    next.push_back(out.back());
    out.swap(next);
  }
}
void RedrawOverlay() {
  if (!g.overlay || !g_gdiplus) return;
  const Int32 width = g.overlayRect.right - g.overlayRect.left, height = g.overlayRect.bottom - g.overlayRect.top;
  if (width <= 0 || height <= 0 || !EnsureSurface(width, height)) return;
  {
    Gdiplus::Graphics graphics(g_surface.dc);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (g.stroke.GetCount() > 1) {
      std::vector<Gdiplus::PointF> points;
      SmoothStroke(g.stroke, points);
      const Gdiplus::Color color = g.mode == Mode::CUT ? Gdiplus::Color(235, 255, 70, 60) : Gdiplus::Color(235, 90, 220, 120);
      Gdiplus::Pen pen(color, 3.0f);
      pen.SetLineJoin(Gdiplus::LineJoinRound);
      pen.SetStartCap(Gdiplus::LineCapRound);
      pen.SetEndCap(Gdiplus::LineCapRound);
      graphics.DrawLines(&pen, points.data(), (INT)points.size());
    }
  }
  POINT position{g.overlayRect.left, g.overlayRect.top};
  SIZE size{width, height};
  POINT source{0, 0};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(g.overlay, nullptr, &position, &size, g_surface.dc, &source, 0, &blend, ULW_ALPHA);
}
void ShowOverlay(HWND canvas) {
  static const wchar_t* kClass = L"NodeStrokeOverlay";
  static Bool registered = false;
  if (!registered) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    registered = RegisterClassW(&wc) != 0;
  }
  GetWindowRect(canvas, &g.overlayRect);
  if (!g.overlay) {
    g.overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g.overlay) return;
  }
  RedrawOverlay();
  SetWindowPos(g.overlay, HWND_TOPMOST, g.overlayRect.left, g.overlayRect.top,
    g.overlayRect.right - g.overlayRect.left, g.overlayRect.bottom - g.overlayRect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
// The window manager shows a layered window's last frame again when it reappears,
// so the overlay is cleared (the stroke is already empty) before hiding.
void HideOverlay() {
  if (!g.overlay) return;
  RedrawOverlay();
  ShowWindow(g.overlay, SW_HIDE);
}

// --- view identification --------------------------------------------------
// After a wheel or middle-drag on a canvas, exactly one view record should change;
// that record belongs to the canvas window.
void CALLBACK TrackTimer(HWND, UINT, UINT_PTR id, DWORD) {
  KillTimer(nullptr, id);
  HWND canvas = g.trackedCanvas;
  g.trackedCanvas = nullptr;
  if (!canvas || !IsCanvasCandidate(canvas) || !RefreshContext() || !(g.contextRoot == g.snapshotRoot)) return;
  iferr (const maxon::BaseArray<ViewSnapshot> now = ReadActiveViews()) return;
  const long index = wire_binding::FindUniqueChanged(g.snapshot, now,
    [](const auto& a, const auto& b) { return a.container == b.container && a.view.key == b.view.key; },
    [](const auto& a, const auto& b) { return a.view.zoom == b.view.zoom && a.view.offset.x == b.view.offset.x && a.view.offset.y == b.view.offset.y; });
  if (index < 0) return;
  const ViewSnapshot* changed = &now[index];
  for (Binding& b : g.bindings) {
    if (b.hwnd == canvas) { b.root = g.contextRoot; b.container = changed->container; b.key = changed->view.key; return; }
  }
  iferr (g.bindings.Append(Binding{canvas, g.contextRoot, changed->container, changed->view.key})) return;
}
void BeginTracking(HWND canvas) {
  g.trackedCanvas = nullptr;
  if (!IsCanvasCandidate(canvas) || !RefreshContext()) return;
  iferr (g.snapshot = ReadActiveViews()) return;
  g.snapshotRoot = g.contextRoot;
  g.trackedCanvas = canvas;
}
void EndTracking() {
  if (g.trackedCanvas) SetTimer(nullptr, 0, 250, TrackTimer);
}

// Drop-insert, defined with the geometry below.
void GrabNode();
void TryDropInsert();
maxon::Result<void> Execute(Mode mode, HWND canvas, const maxon::BaseArray<ScreenPoint>& screen);
// Why a gesture did not act, appended to <prefs>/nodestroke_debug.txt when the
// diagnostic log is turned on in the preferences.
void DebugLog(const maxon::String& text) {
  iferr_scope_handler { return; };
  if (!wire_prefs::Enabled(NODESTROKE_PREFS_LOG)) return;
  const maxon::BaseArray<Char> bytes = (FormatString("@ ", (Int64)GetTickCount64()) + text + "\n"_s).GetCString() iferr_return;
  Int length = bytes.GetCount();
  if (length > 0 && bytes[length - 1] == 0) --length;
  const Filename path = GeGetC4DPath(C4D_PATH_PREFS) + Filename("nodestroke_debug.txt"_s);
  AutoAlloc<BaseFile> file;
  if (file && file->Open(path, GeFExist(path) ? FILEOPEN::APPEND : FILEOPEN::WRITE)) file->WriteBytes(bytes.GetFirst(), length);
}
// Watcher thread: while a canvas drag runs, notices when the cursor rests and wakes
// the hook. It touches no graph or C4D state.
DWORD WINAPI WatchDwell(void*) {
  POINT last{-1, -1};
  ULONGLONG since = 0;
  Bool fired = false;
  while (!g.stopWatcher) {
    if (!g.watchDrag) {
      fired = false;
      last = POINT{-1, -1};
      WaitForSingleObject(g.dragEvent, INFINITE);
      continue;
    }
    Sleep(30);
    POINT p;
    if (!GetCursorPos(&p)) continue;
    const ULONGLONG now = GetTickCount64();
    if (p.x != last.x || p.y != last.y) { last = p; since = now; fired = false; continue; }
    if (fired || now - since < kDwellMs) continue;
    fired = true;
    HWND canvas = g.watchCanvas;
    POINT client = p;
    if (canvas && ScreenToClient(canvas, &client)) PostMessageW(canvas, WM_MOUSEMOVE, MK_LBUTTON | kDwellMarker, MAKELPARAM(client.x, client.y));
  }
  return 0;
}
// Counts horizontal direction reversals with a minimum excursion, like a shake.
Bool FeedShake(Int32 x) {
  if (g.shakeDirection == 0) {
    if (Abs(x - g.shakePivot) >= kShakePixels) { g.shakeDirection = x > g.shakePivot ? 1 : -1; g.shakeExtreme = x; }
  } else if ((g.shakeDirection > 0 && x > g.shakeExtreme) || (g.shakeDirection < 0 && x < g.shakeExtreme)) {
    g.shakeExtreme = x;
  } else if (Abs(x - g.shakeExtreme) >= kShakePixels) {
    g.shakeDirection = -g.shakeDirection;
    g.shakeExtreme = x;
    if (++g.shakeReversals >= kShakeReversals) return true;
  }
  return false;
}

// --- stroke capture ------------------------------------------------------------
// The stroke runs while the gesture key is held: the cursor is sampled on a timer
// (no mouse button, so no capture is needed) and the key release applies it.
void FinishStroke() {
  if (g.pollTimer) KillTimer(nullptr, g.pollTimer);
  g.pollTimer = 0;
  g.pendingMode = g.mode;
  g.pendingCanvas = g.canvas;
  g.pendingRoot = g.strokeRoot;
  g.pending = std::move(g.stroke);
  g.stroke.Flush();
  HideOverlay();
  g.mode = Mode::NONE;
  g.key = 0;
  SpecialEventAdd(kGestureMessageId);
}
void CALLBACK PollStroke(HWND, UINT, UINT_PTR, DWORD) {
  if (g.mode == Mode::NONE) return;
  POINT cursor;
  if (GetCursorPos(&cursor)) {
    const ScreenPoint last = g.stroke.IsEmpty() ? ScreenPoint{-1, -1} : g.stroke[g.stroke.GetCount() - 1];
    if (cursor.x != last.x || cursor.y != last.y) {
      iferr (g.stroke.Append(ScreenPoint{cursor.x, cursor.y})) {}
      RedrawOverlay();
    }
  }
  if (!(GetAsyncKeyState((int)g.key) & 0x8000)) FinishStroke();
}
void StartStroke(Mode mode, WPARAM key, HWND canvas) {
  g.strokeRoot = g.contextRoot;
  g.mode = mode;
  g.key = key;
  g.canvas = canvas;
  g.stroke.Flush();
  POINT cursor;
  if (GetCursorPos(&cursor)) { iferr (g.stroke.Append(ScreenPoint{cursor.x, cursor.y})) {} }
  ShowOverlay(canvas);
  g.pollTimer = SetTimer(nullptr, 0, 10, PollStroke);
}

// --- message hook -----------------------------------------------------------
LRESULT CALLBACK GestureHook(int code, WPARAM wParam, LPARAM lParam) {
  if (code != HC_ACTION || wParam != PM_REMOVE || !lParam || !g.enabled)
    return CallNextHookEx(g.hook, code, wParam, lParam);
  MSG* msg = reinterpret_cast<MSG*>(lParam);
  switch (msg->message) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR: {
      const WPARAM key = msg->message == WM_CHAR ? (WPARAM)(UINT_PTR)CharUpperA((LPSTR)msg->wParam) : msg->wParam;
      if (key != 'Y' && key != 'I') break;
      if (g.mode != Mode::NONE) {
        if (msg->message == WM_KEYUP && key == g.key) FinishStroke();
        msg->message = WM_NULL;
        break;
      }
      if ((key == 'I' && !g.connect) || (key == 'Y' && !g.cut)) break;
      HWND canvas = CanvasUnderCursor();
      if (!canvas) break;
      if (!HasBoundCanvas(canvas)) {
        if (msg->message == WM_KEYDOWN && !(msg->lParam & (1 << 30))) {
          DebugLog("gesture: key down on an unbound canvas"_s);
          StatusSetText(GeLoadString(IDS_GESTURE_UNKNOWN_EDITOR));
        }
        break;
      }
      // Keep the gesture keys from reaching the editor while the cursor is on a canvas.
      const Bool repeat = (msg->lParam & (1 << 30)) != 0;
      if (msg->message == WM_KEYDOWN && !repeat) StartStroke(key == 'Y' ? Mode::CUT : Mode::CONNECT, key, canvas);
      msg->message = WM_NULL;
      break;
    }
    case WM_MOUSEWHEEL:
    case WM_MBUTTONDOWN: {
      // Wheel messages go to the focus window; use the window under the cursor.
      HWND under = WindowFromPoint(msg->pt);
      if (IsCanvasCandidate(under)) BeginTracking(under);
      if (msg->message == WM_MOUSEWHEEL) EndTracking();
      break;
    }
    case WM_MBUTTONUP:
      EndTracking();
      break;
    case WM_LBUTTONDOWN:
      if (g.mode == Mode::NONE && HasBoundCanvas(msg->hwnd)) {
        g.dragCanvas = msg->hwnd;
        g.dragRoot = g.contextRoot;
        g.lastMove = ScreenPoint{msg->pt.x, msg->pt.y};
        g.inserted = false;
        g.dragging = true;
        g.shaken = false;
        g.dragStart = ScreenPoint{msg->pt.x, msg->pt.y};
        g.shakePivot = g.shakeExtreme = msg->pt.x;
        g.shakeDirection = 0;
        g.shakeReversals = 0;
        GrabNode();
        g.watchCanvas = msg->hwnd;
        g.watchDrag = true;
        if (g.dragEvent) SetEvent(g.dragEvent);
      } else if (!IsCanvasCandidate(msg->hwnd)) {
        // Clicking editor chrome can change the displayed group (breadcrumbs,
        // tabs, etc.). Require a fresh scroll in the newly displayed view.
        g.bindings.Flush();
      }
      break;
    case WM_LBUTTONDBLCLK:
      // Entering a group can reuse the same HWND and material graph.
      if (IsCanvasCandidate(msg->hwnd)) g.bindings.Flush();
      break;
    case WM_MOUSEMOVE: {
      if (msg->wParam & kDwellMarker) {
        msg->message = WM_NULL;  // our own wake-up, never seen by the editor
        const Bool moved = Abs(msg->pt.x - g.dragStart.x) + Abs(msg->pt.y - g.dragStart.y) > 6;
        if (g.drop && g.dragging && moved && !g.shaken && !g.inserted && HasBoundCanvas(g.dragCanvas) && g.contextRoot == g.dragRoot) TryDropInsert();
        break;
      }
      if (!g.dragging || !(msg->wParam & MK_LBUTTON)) break;
      if (msg->pt.x == g.lastMove.x && msg->pt.y == g.lastMove.y) break;
      g.lastMove = ScreenPoint{msg->pt.x, msg->pt.y};
      if (g.shake && !g.shaken && !g.inserted && FeedShake(msg->pt.x) && HasBoundCanvas(g.dragCanvas) && g.contextRoot == g.dragRoot) {
        // Detach right away, while the node is still being dragged.
        g.shaken = true;
        maxon::BaseArray<ScreenPoint> start;
        iferr (start.Append(g.dragStart)) {}
        iferr (Execute(Mode::SHAKE, g.dragCanvas, start)) { StatusSetText(err.GetMessage()); }
        EventAdd();
      }
      break;
    }
    case WM_LBUTTONUP:
      g.dragging = false;
      g.watchDrag = false;
      if (g.dragEvent) ResetEvent(g.dragEvent);
      g.grabNode = maxon::GraphNode();
      break;
  }
  return CallNextHookEx(g.hook, code, wParam, lParam);
}

// --- geometry --------------------------------------------------------------
struct Vec { Float x, y; };
struct NodeGeom {
  maxon::GraphNode node;
  Vec pos;
  Float width = 0;
  maxon::BaseArray<maxon::GraphNode> outs, ins;  // visible ports in display order
  maxon::BaseArray<Float> outY, inY;             // row centers below the node top
  Float content = kRow;                          // height of all port and group rows
  Float above = kAboveTitle;                     // node area above the record y
  // Set when the canvas drawing showed this node: its title-to-bottom extent is then
  // measured, not estimated, and the port rows are where their dots were drawn.
  Bool drawn = false;
  Float top = 0, bottom = 0;
  Float Height() const { return drawn ? bottom - pos.y : kHeader + content + kPadding; }
};

maxon::Result<Bool> Connected(const maxon::GraphNode& port, maxon::PORT_DIR dir) {
  iferr_scope;
  Bool any = false;
  port.GetConnections(dir, [&any](const maxon::GraphConnection&) -> maxon::Result<Bool> { any = true; return false; }) iferr_return;
  return any;
}
// Ports the editor shows: not flagged hideportinnodegraph, or connected.
maxon::Result<maxon::BaseArray<maxon::GraphNode>> VisiblePorts(const maxon::GraphNode& list, maxon::NODE_KIND kind, maxon::PORT_DIR dir) {
  iferr_scope;
  maxon::BaseArray<maxon::GraphNode> ports;
  maxon::BaseArray<Int> order;
  maxon::InternedId hideId, alwaysHideId;
  hideId.Init("net.maxon.node.attribute.hideportinnodegraph") iferr_return;
  alwaysHideId.Init("net.maxon.node.attribute.alwayshideportinnodegraph") iferr_return;
  list.GetChildren([&](const maxon::GraphNode& p) -> maxon::Result<Bool> {
    iferr_scope;
    const Bool alwaysHidden = p.GetValue<Bool>(alwaysHideId).GetValueOr(false) iferr_return;
    if (alwaysHidden) return true;
    const Bool hidden = p.GetValue<Bool>(hideId).GetValueOr(false) iferr_return;
    if (hidden) {
      const Bool connected = Connected(p, dir) iferr_return;
      if (!connected) return true;
    }
    const Int index = p.GetValue(maxon::nodes::ReqOrderIndex).GetOrDefault() iferr_return;
    ports.Append(p) iferr_return;
    order.Append(index) iferr_return;
    return true;
  }, kind) iferr_return;
  for (Int i = 1; i < ports.GetCount(); ++i) {
    for (Int j = i; j > 0 && order[j - 1] > order[j]; --j) {
      maxon::Swap(order[j - 1], order[j]);
      maxon::Swap(ports[j - 1], ports[j]);
    }
  }
  return ports;
}
// A texture sampler with a file shows its image as a thumbnail above the ports.
maxon::Result<Bool> HasTextureImage(const maxon::GraphNode& node) {
  iferr_scope;
  const maxon::GraphNode inputs = node.GetInputs() iferr_return;
  maxon::InternedId tex0, path;
  tex0.Init("com.redshift3d.redshift4c4d.nodes.core.texturesampler.tex0") iferr_return;
  path.Init("path") iferr_return;
  const maxon::GraphNode bundle = inputs.FindChild(tex0) iferr_return;
  if (!bundle.IsValid()) return false;
  const maxon::GraphNode port = bundle.FindChild(path) iferr_return;
  if (!port.IsValid()) return false;
  const maxon::Url url = port.GetPortValue<maxon::Url>().GetValueOr(maxon::Url()) iferr_return;
  return !url.IsEmpty();
}
// Orders the visible inputs as the node's UI description lists them and inserts a
// group title row before the first visible port of every titled group.
maxon::Result<void> ArrangeRows(NodeGeom& geom) {
  iferr_scope;
  Float y = 0;
  for (Int i = 0; i < geom.outs.GetCount(); ++i) { geom.outY.Append(y + kRow * 0.5) iferr_return; y += kRow; }
  maxon::BaseArray<maxon::GraphNode> ordered;
  BaseDocument* doc = GetActiveDocument();
  const maxon::IdAndVersion asset = geom.node.GetValue(maxon::nodes::AssetId).GetOrDefault() iferr_return;
  maxon::BaseArray<maxon::DataDictionary> entries;
  if (doc) {
    iferr (const maxon::AssetRepositoryRef& repository = doc->GetSceneRepository(true)) {}
    else {
      iferr (const maxon::DataDescription ui = repository.LoadDescription(maxon::DATADESCRIPTION_CATEGORY_UI, maxon::Resource::GetCurrentLanguage(), asset.first)) {}
      else { iferr (entries = ui.GetEntries()) {} }
    }
  }
  struct Group { maxon::InternedId id, parent; Bool titled = false, shown = false; };
  maxon::BaseArray<Group> groups;
  auto findGroup = [&groups](const maxon::InternedId& id) -> Group* {
    for (Group& group : groups) if (group.id == id) return &group;
    return nullptr;
  };
  for (const maxon::DataDictionary& e : entries) {
    const maxon::InternedId id = e.Get(maxon::DESCRIPTION::BASE::IDENTIFIER, maxon::InternedId());
    const maxon::InternedId parent = e.Get(maxon::DESCRIPTION::UI::BASE::GROUPID, maxon::InternedId());
    if (e.Contains(maxon::DESCRIPTION::UI::BASE::SHOWGROUPINPORTLIST)) {
      groups.Append(Group{id, parent, e.Get(maxon::DESCRIPTION::UI::BASE::SHOWGROUPINPORTLIST, false), false}) iferr_return;
      continue;
    }
    for (Int i = 0; i < geom.ins.GetCount(); ++i) {
      if (!(geom.ins[i].GetId() == id)) continue;
      // Only the port's own group gets a title row (outer groups are not shown).
      Group* own = findGroup(parent);
      if (own && !own->shown) {
        own->shown = true;
        if (own->titled) y += kGroupHeader;
      }
      ordered.Append(geom.ins[i]) iferr_return;
      geom.inY.Append(y + kRow * 0.5) iferr_return;
      y += kRow;
    }
  }
  // Ports the description does not list (e.g. added at runtime) follow in editor order.
  for (const maxon::GraphNode& in : geom.ins) {
    Bool listed = false;
    for (const maxon::GraphNode& o : ordered) if (o == in) listed = true;
    if (listed) continue;
    ordered.Append(in) iferr_return;
    geom.inY.Append(y + kRow * 0.5) iferr_return;
    y += kRow;
  }
  geom.ins = std::move(ordered);
  geom.content = Max(y, kRow);
  return maxon::OK;
}

maxon::Result<maxon::BaseArray<NodeGeom>> Layout(const maxon::GraphNode& container) {
  iferr_scope;
  maxon::BaseArray<NodeGeom> nodes;
  maxon::BaseArray<maxon::GraphNode> raw;
  container.GetChildren([&raw](const maxon::GraphNode& n) -> maxon::Result<Bool> {
    iferr_scope;
    raw.Append(n) iferr_return;
    return true;
  }, maxon::NODE_KIND::NODE) iferr_return;
  for (const maxon::GraphNode& n : raw) {
    iferr (const maxon::BaseArray<wire_native::NodeRecord> records = wire_native::ReadNodePositions(n)) continue;
    if (records.IsEmpty()) continue;
    NodeGeom geom;
    geom.node = n;
    geom.pos = Vec{records[0].position.x, records[0].position.y};
    geom.width = records[0].width;
    {
      const maxon::IdAndVersion asset = n.GetValue(maxon::nodes::AssetId).GetOrDefault() iferr_return;
      const maxon::String id = asset.first.ToString();
      if (id.FindIndex("material"_s) >= 0 || id.FindIndex(".node.output"_s) >= 0) geom.above = kAbovePreview;
      else if (id.EndsWith(".texturesampler"_s)) {
        const Bool image = HasTextureImage(n) iferr_return;
        if (image) geom.above = kAbovePreview;
      }
    }
    const maxon::GraphNode outputs = n.GetOutputs() iferr_return;
    const maxon::GraphNode inputs = n.GetInputs() iferr_return;
    geom.outs = VisiblePorts(outputs, maxon::NODE_KIND::OUTPORT, maxon::PORT_DIR::OUTPUT) iferr_return;
    geom.ins = VisiblePorts(inputs, maxon::NODE_KIND::INPORT, maxon::PORT_DIR::INPUT) iferr_return;
    ArrangeRows(geom) iferr_return;
    nodes.Append(std::move(geom)) iferr_return;
  }
  return nodes;
}

Float Cross(Vec a, Vec b, Vec c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }
Bool SegmentsCross(Vec a, Vec b, Vec c, Vec d) {
  const Float d1 = Cross(a, b, c), d2 = Cross(a, b, d), d3 = Cross(c, d, a), d4 = Cross(c, d, b);
  return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}
Bool PolylinesCross(const maxon::BaseArray<Vec>& a, const maxon::BaseArray<Vec>& b) {
  for (Int i = 1; i < a.GetCount(); ++i)
    for (Int j = 1; j < b.GetCount(); ++j)
      if (SegmentsCross(a[i - 1], a[i], b[j - 1], b[j])) return true;
  return false;
}
// The title row and port list are certain; the preview area above material and
// output nodes is estimated, so it only counts where no other node is hit.
Float SegmentDistance(Vec p, Vec a, Vec b) {
  const Float dx = b.x - a.x, dy = b.y - a.y, len = dx * dx + dy * dy;
  const Float t = len > 0 ? Max(0.0, Min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len)) : 0.0;
  const Float ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
  return Sqrt(ex * ex + ey * ey);
}
Float PolylineDistance(Vec p, const maxon::BaseArray<Vec>& line) {
  Float best = MAXVALUE_FLOAT64;
  for (Int i = 1; i < line.GetCount(); ++i) best = Min(best, SegmentDistance(p, line[i - 1], line[i]));
  return best;
}
// A cut counts only where the stroke clearly passes through the wire: before and
// after the crossing it gets at least `margin` away from the wire. Strokes that only
// graze a neighbouring wire (whose rounded corners are approximated) leave it alone.
Bool StrokeCutsWire(const maxon::BaseArray<Vec>& stroke, const maxon::BaseArray<Vec>& wire, Float margin) {
  for (Int i = 1; i < stroke.GetCount(); ++i) {
    Bool crossed = false;
    for (Int j = 1; j < wire.GetCount() && !crossed; ++j) crossed = SegmentsCross(stroke[i - 1], stroke[i], wire[j - 1], wire[j]);
    if (!crossed) continue;
    Bool before = false, after = false;
    for (Int k = i - 1; k >= 0 && !before; --k) before = PolylineDistance(stroke[k], wire) >= margin;
    for (Int k = i; k < stroke.GetCount() && !after; ++k) after = PolylineDistance(stroke[k], wire) >= margin;
    if (before && after) return true;
  }
  return false;
}
const NodeGeom* HitNode(const maxon::BaseArray<NodeGeom>& nodes, Vec p) {
  const NodeGeom* extended = nullptr;
  for (const NodeGeom& n : nodes) {
    if (p.x < n.pos.x - kEdgeSlack || p.x > n.pos.x + n.width + kEdgeSlack || p.y > n.pos.y + n.Height()) continue;
    if (n.drawn) {
      if (p.y >= n.top) return &n;
      continue;
    }
    if (p.y >= n.pos.y - kTitleRow) return &n;
    if (!extended && p.y >= n.pos.y - n.above) extended = &n;
  }
  return extended;
}
// Dot position of a visible port: outputs on the right edge, inputs on the left.
Vec PortPoint(const NodeGeom& n, Int index, Bool output) {
  const Float y = output ? n.outY[index] : n.inY[index];
  return Vec{output ? n.pos.x + n.width : n.pos.x, n.pos.y + kHeader + y};
}
// Visible port on the given side whose row contains p, or an invalid node.
maxon::GraphNode PortAt(const NodeGeom& n, Vec p, Bool output) {
  const maxon::BaseArray<maxon::GraphNode>& ports = output ? n.outs : n.ins;
  Int best = -1;
  Float bestDistance = kRow * 0.5;
  for (Int i = 0; i < ports.GetCount(); ++i) {
    const Float distance = Abs(PortPoint(n, i, output).y - p.y);
    if (distance < bestDistance) { best = i; bestDistance = distance; }
  }
  return best >= 0 ? ports[best] : maxon::GraphNode();
}
// Wires are drawn as a straight line between port dots that leaves and enters them
// horizontally through a rounded corner (stylesheet wireStyle: cornerMaxRadius 12,
// startSegmentMinLength 0). The corners are approximated by a short horizontal stub.
constexpr Float kWireStub = 6.0;
maxon::Result<maxon::BaseArray<Vec>> WireCurve(Vec start, Vec end) {
  iferr_scope;
  maxon::BaseArray<Vec> curve;
  curve.Append(start) iferr_return;
  curve.Append(Vec{start.x + kWireStub, start.y}) iferr_return;
  curve.Append(Vec{end.x - kWireStub, end.y}) iferr_return;
  curve.Append(end) iferr_return;
  return curve;
}
struct WireGeom { maxon::GraphNode out, in; maxon::BaseArray<Vec> curve; };
struct BoundaryPort { maxon::GraphNode port; Vec pos; };
struct BoundaryPorts {
  maxon::BaseArray<BoundaryPort> sources, targets;
};

// Verify that each presenter-side port belongs to the bound graph container and
// return the ports of each side in presenter order, which is the order they are drawn.
// GetGraphNode is a trivial, version-checked accessor in the supported build:
// it returns the GraphNode stored at component offset 376 + 8. Do not invoke
// an undocumented method; use the public GraphNode API on the verified object.
Bool PresentedPortsMatch(wire_group_native::Address model, const maxon::BaseArray<maxon::GraphNode>& ins,
                         const maxon::BaseArray<maxon::GraphNode>& outs, maxon::BaseArray<maxon::GraphNode>& presentedIns,
                         maxon::BaseArray<maxon::GraphNode>& presentedOuts) {
  using namespace wire_group_native;
  presentedIns.Flush();
  presentedOuts.Flush();
  const Address module = reinterpret_cast<Address>(GetModuleHandleW(L"c4d_nodeeditor.xdl64"));
  const Address infoOffset = module ? Word(module + 0x28f750) : 0;
  if (!infoOffset || infoOffset > 0x100000) { DebugLog("group presenter: invalid interface offset"_s); return false; }
  std::unordered_set<Address> visited;
  // Port lists are named from inside the group: a group output receives inner
  // wires, so it is presented in an in-port list. Each list must match one side.
  // Folders in a side panel are port groups whose children are ports or folders.
  struct Walker {
    const maxon::BaseArray<maxon::GraphNode>& ins;
    const maxon::BaseArray<maxon::GraphNode>& outs;
    Address infoOffset;
    std::unordered_set<Address>& visited;
    const char* portClass;
    const char* groupClass;
    const maxon::BaseArray<maxon::GraphNode>* side = nullptr;
    maxon::BaseArray<maxon::GraphNode> ordered;
    Bool Visit(Address p, Int depth) {
      if (depth > 16) { DebugLog("group presenter: folders nested too deep"_s); return false; }
      for (; p; p = Word(p + 0xb8)) {
        if (!visited.insert(p).second || visited.size() > 4096) { DebugLog("group presenter: presenter loop"_s); return false; }
        if (ClassIs(p, groupClass)) {
          if (!Visit(Word(p + 0xd0), depth + 1)) return false;
          continue;
        }
        if (!ClassIs(p, portClass)) { DebugLog("group presenter: unexpected port type"_s); return false; }
        const Address ci = Word(p), size = Word(ci + 0x20);
        unsigned char valid = 0, code[5]{};
        Address fn = 0, delta = 0;
        if (size <= infoOffset || !ReadValue(ci + infoOffset, valid) || valid != 1 ||
            !ReadValue(ci + infoOffset + 1, fn) || !ReadValue(ci + infoOffset + 9, delta) || delta != 376 ||
            !Read(fn, code, sizeof(code)) || std::memcmp(code, "\x48\x8d\x41\x08\xc3", 5) != 0 ||
            !ClassIs(Word(p + 0x180), "net.maxon.node.graphmodel.class.lockedco")) { DebugLog("group presenter: accessor or graph type mismatch"_s); return false; }
        const auto* graphNode = reinterpret_cast<const maxon::GraphNode*>(p + 0x180);
        if (!graphNode->IsValid()) { DebugLog("group presenter: invalid graph node"_s); return false; }
        auto find = [&](const maxon::BaseArray<maxon::GraphNode>& candidates) -> const maxon::GraphNode* {
          const maxon::GraphNode* found = nullptr;
          for (const auto& port : candidates) {
            if (!(port.GetPath() == graphNode->GetPath())) continue;
            if (found) return nullptr;
            found = &port;
          }
          return found;
        };
        if (!side) side = find(ins) ? &ins : find(outs) ? &outs : nullptr;
        const maxon::GraphNode* port = side ? find(*side) : nullptr;
        if (!port) { DebugLog(FormatString("group presenter: path mismatch, presenter=@", graphNode->GetPath())); return false; }
        iferr (ordered.Append(*port)) return false;
      }
      return true;
    }
  };
  Address node = Word(model + 0xd0);
  for (; node && visited.insert(node).second && visited.size() <= 4096; node = Word(node + 0xb8)) {
    if (!ClassIs(node, "net.maxon.mvp.presenterclasses.graphionode")) continue;
    const Address list = Word(node + 0xd0);
    const bool isIn = ClassIs(list, "net.maxon.mvp.presenterclasses.graphinportlist");
    const bool isOut = ClassIs(list, "net.maxon.mvp.presenterclasses.graphoutportlist");
    if (!isIn && !isOut) { DebugLog("group presenter: unexpected port list"_s); return false; }
    Walker walker{ins, outs, infoOffset, visited,
      isIn ? "net.maxon.mvp.presenterclasses.graphinport" : "net.maxon.mvp.presenterclasses.graphoutport",
      isIn ? "net.maxon.mvp.presenterclasses.graphinportgroup" : "net.maxon.mvp.presenterclasses.graphoutportgroup"};
    if (!walker.Visit(Word(list + 0xd0), 0)) return false;
    if (!walker.side) continue;
    auto& dest = walker.side == &ins ? presentedIns : presentedOuts;
    if (!dest.IsEmpty()) { DebugLog("group presenter: two lists for one side"_s); return false; }
    iferr (dest.CopyFrom(walker.ordered)) return false;
  }
  if (node || presentedIns.GetCount() != ins.GetCount() || presentedOuts.GetCount() != outs.GetCount()) {
    DebugLog(FormatString("group presenter: counts in=@/@ out=@/@ unfinished=@", presentedIns.GetCount(), ins.GetCount(),
      presentedOuts.GetCount(), outs.GetCount(), node != 0));
    return false;
  }
  return true;
}

maxon::Result<BoundaryPorts> GroupBoundary(const maxon::GraphNode& container, const maxon::GraphNode& graphRoot,
                                           const wire_group_native::Drawing& drawing, const wire_native::ViewRecord& view) {
  iferr_scope;
  BoundaryPorts result;
  if (container == graphRoot) return result;
  const maxon::GraphNode inputs = container.GetInputs() iferr_return;
  const maxon::GraphNode outputs = container.GetOutputs() iferr_return;
  const auto ins = wire_ops::EditorOrderPorts(inputs, maxon::NODE_KIND::INPORT) iferr_return;
  const auto outs = wire_ops::EditorOrderPorts(outputs, maxon::NODE_KIND::OUTPORT) iferr_return;
  wire_group_native::Snapshot snapshot;
  if (!wire_group_native::Capture(drawing, snapshot)) {
    DebugLog("group: native capture unavailable: "_s + maxon::String(snapshot.failure ? snapshot.failure : "unknown"));
    return result;
  }
  maxon::BaseArray<maxon::GraphNode> presentedIns, presentedOuts;
  if (!PresentedPortsMatch(snapshot.model, ins, outs, presentedIns, presentedOuts)) {
    DebugLog("group: presenter and bound group do not match"_s);
    return result;
  }
  auto map = [&](const auto& circles, const auto& ports, auto& dest) -> maxon::Result<void> {
    iferr_scope;
    std::vector<wire_group_draw::Port> names;
    for (const auto& port : ports) {
      const maxon::String label = port.GetValue(maxon::EffectiveName).GetOrDefault() iferr_return;
      const auto labelBytes = label.GetCString() iferr_return;
      const auto idBytes = port.GetId().ToString().GetCString() iferr_return;
      auto copyUtf8 = [](const auto& bytes) {
        Int length = bytes.GetCount();
        if (length && bytes[length - 1] == 0) --length;
        return length ? std::string(bytes.GetFirst(), static_cast<std::size_t>(length)) : std::string();
      };
      names.push_back({copyUtf8(idBytes), copyUtf8(labelBytes)});
    }
    std::vector<wire_group_draw::MatchedPort> matched;
    // Ports are drawn in presenter order. When the drawn labels equal the presented
    // labels one by one, that order also tells same-named ports apart; otherwise
    // only unique full labels are matched.
    if (!wire_group_draw::MatchInOrder(circles, names, matched) && !wire_group_draw::Match(circles, names, matched)) {
      DebugLog(FormatString("group: @ drawn labels do not match @ ports", Int(circles.size()), ports.GetCount()));
      return maxon::OK;
    }
    for (const auto& entry : matched) {
      dest.Append(BoundaryPort{ports[Int(entry.port)], Vec{(entry.x - view.offset.x) / view.zoom, (entry.y - view.offset.y) / view.zoom}}) iferr_return;
    }
    return maxon::OK;
  };
  map(snapshot.panels.left, presentedIns, result.sources) iferr_return;
  map(snapshot.panels.right, presentedOuts, result.targets) iferr_return;
  return result;
}
maxon::Result<maxon::BaseArray<Vec>> BoundaryWire(Vec a, Vec b) {
  iferr_scope;
  maxon::BaseArray<Vec> points;
  points.Append(a) iferr_return;
  points.Append(b) iferr_return;
  return points;
}
maxon::Result<maxon::BaseArray<WireGeom>> Wires(const maxon::BaseArray<NodeGeom>& nodes, const BoundaryPorts& boundary) {
  iferr_scope;
  maxon::BaseArray<WireGeom> wires;
  for (const NodeGeom& target : nodes) {
    for (Int j = 0; j < target.ins.GetCount(); ++j) {
      const maxon::GraphNode& in = target.ins[j];
      maxon::BaseArray<maxon::GraphNode> sources;
      in.GetConnections(maxon::PORT_DIR::INPUT, [&sources](const maxon::GraphConnection& c) -> maxon::Result<Bool> {
        iferr_scope;
        sources.Append(c.first) iferr_return;
        return true;
      }) iferr_return;
      for (const maxon::GraphNode& out : sources) {
        Bool fromBoundary = false;
        for (const auto& source : boundary.sources) if (out == source.port) {
          WireGeom wire{out, in, {}};
          wire.curve = BoundaryWire(source.pos, PortPoint(target, j, false)) iferr_return;
          wires.Append(std::move(wire)) iferr_return;
          fromBoundary = true;
          break;
        }
        if (fromBoundary) continue;
        const maxon::GraphNode owner = out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
        for (const NodeGeom& source : nodes) {
          if (!(source.node == owner)) continue;
          Int row = -1;
          for (Int k = 0; k < source.outs.GetCount(); ++k) if (source.outs[k] == out) row = k;
          // Do not guess a wire's origin when its output row is hidden or unknown.
          if (row < 0) continue;
          WireGeom wire{out, in, {}};
          wire.curve = WireCurve(PortPoint(source, row, true), PortPoint(target, j, false)) iferr_return;
          wires.Append(std::move(wire)) iferr_return;
        }
      }
    }
  }
  for (const auto& boundaryTarget : boundary.targets) {
    maxon::BaseArray<maxon::GraphNode> sources;
    boundaryTarget.port.GetConnections(maxon::PORT_DIR::INPUT, [&sources](const maxon::GraphConnection& c) -> maxon::Result<Bool> {
      iferr_scope;
      sources.Append(c.first) iferr_return;
      return true;
    }) iferr_return;
    for (const maxon::GraphNode& out : sources) {
      Bool fromBoundary = false;
      for (const auto& source : boundary.sources) if (out == source.port) {
        WireGeom wire{out, boundaryTarget.port, {}};
        wire.curve = BoundaryWire(source.pos, boundaryTarget.pos) iferr_return;
        wires.Append(std::move(wire)) iferr_return;
        fromBoundary = true;
        break;
      }
      if (fromBoundary) continue;
      for (const NodeGeom& source : nodes) {
        for (Int i = 0; i < source.outs.GetCount(); ++i) {
          if (!(source.outs[i] == out)) continue;
          WireGeom wire{out, boundaryTarget.port, {}};
          wire.curve = BoundaryWire(PortPoint(source, i, true), boundaryTarget.pos) iferr_return;
          wires.Append(std::move(wire)) iferr_return;
        }
      }
    }
  }
  return wires;
}
const ViewSnapshot* ViewFor(HWND canvas, const maxon::BaseArray<ViewSnapshot>& views) {
  if (!HasBoundCanvas(canvas)) return nullptr;
  for (const Binding& b : g.bindings) {
    if (b.hwnd != canvas || !(b.root == g.contextRoot)) continue;
    for (const auto& r : views) if (r.container == b.container && r.view.key == b.key) return &r;
  }
  return nullptr;
}

// The graph container and view a canvas shows, and what it last drew.
struct CanvasView {
  const ViewSnapshot* active = nullptr;
  wire_group_native::Drawing drawing;  // no window when the native drawing is unavailable
  std::vector<wire_group_draw::DrawnNode> drawn;
};
// Drawn body that shows the node at `record` under `view`: same left edge and width,
// and the record's port-list top inside the body. Bodies are rounded by 8 * zoom.
const wire_group_draw::DrawnNode* DrawnBody(const std::vector<wire_group_draw::DrawnNode>& drawn,
                                            const wire_native::ViewRecord& view, Vec pos, Float width) {
  const Float x = pos.x * view.zoom + view.offset.x, y = (pos.y + kHeader) * view.zoom + view.offset.y;
  const Float w = width * view.zoom;
  const wire_group_draw::DrawnNode* found = nullptr;
  for (const auto& b : drawn) {
    if (Abs(b.radius - 8.0 * view.zoom) > 0.05 || Abs(b.x - x) > 1.0 || Abs(b.w - w) > 1.0 || y < b.y || y > b.y + b.h) continue;
    if (found) return nullptr;
    found = &b;
  }
  return found;
}
// Native canvases are identified by geometry: among all views of the active graph,
// the one whose node layout predicts the bodies this canvas drew is the one it
// shows. A graph the canvas does not show (another material) matches nothing.
// Without a native drawing (other C4D builds) the scroll binding is used.
maxon::Result<void> ResolveView(HWND canvas, const maxon::BaseArray<ViewSnapshot>& views, CanvasView& result) {
  iferr_scope;
  result.active = nullptr;
  result.drawn.clear();
  if (!wire_group_native::ReadDrawing(canvas, result.drawing)) {
    result.active = ViewFor(canvas, views);
    return maxon::OK;
  }
  result.drawn = wire_group_draw::ParseNodes(result.drawing.commands, wire_group_native::ReadLabel);
  Int bestScore = 0;
  Bool tie = false;
  for (const ViewSnapshot& candidate : views) {
    Int score = 0;
    candidate.container.GetChildren([&](const maxon::GraphNode& n) -> maxon::Result<Bool> {
      iferr_scope;
      iferr (const maxon::BaseArray<wire_native::NodeRecord> records = wire_native::ReadNodePositions(n)) return true;
      if (!records.IsEmpty() && DrawnBody(result.drawn, candidate.view, Vec{records[0].position.x, records[0].position.y}, records[0].width)) ++score;
      return true;
    }, maxon::NODE_KIND::NODE) iferr_return;
    if (score == 0 || score < bestScore) continue;
    if (score == bestScore && result.active) {
      // Two records of one container with the same transform show the same thing.
      const auto& a = result.active->view;
      const auto& b = candidate.view;
      tie = tie || !(result.active->container == candidate.container) || Abs(a.zoom - b.zoom) > 1e-4 ||
        Abs(a.offset.x - b.offset.x) > 0.01 || Abs(a.offset.y - b.offset.y) > 0.01;
      continue;
    }
    bestScore = score;
    result.active = &candidate;
    tie = false;
  }
  if (tie) result.active = nullptr;
  // A view without nodes (an empty group) can only be known from a scroll binding.
  if (!result.active) result.active = ViewFor(canvas, views);
  if (!result.active) DebugLog(FormatString("view: no unique view matches the drawing (bodies=@, views=@)", Int(result.drawn.size()), views.GetCount()));
  return maxon::OK;
}
// Replaces estimated node extents and port rows with what the canvas drew, where the
// drawn port labels identify the node's shown ports. Other nodes keep the estimate.
maxon::Result<void> ApplyDrawn(maxon::BaseArray<NodeGeom>& nodes, const CanvasView& canvas) {
  iferr_scope;
  if (!canvas.active || canvas.drawn.empty()) return maxon::OK;
  const wire_native::ViewRecord& view = canvas.active->view;
  auto toGraphY = [&view](Float y) { return (y - view.offset.y) / view.zoom; };
  auto match = [&](NodeGeom& n, const std::vector<wire_group_draw::Circle>& circles, maxon::BaseArray<maxon::GraphNode>& ports,
                   maxon::BaseArray<Float>& rows) -> maxon::Result<Bool> {
    iferr_scope;
    if (circles.empty()) return ports.IsEmpty();
    std::vector<wire_group_draw::Port> names;
    for (const auto& port : ports) {
      const maxon::String label = port.GetValue(maxon::EffectiveName).GetOrDefault() iferr_return;
      const auto bytes = label.GetCString() iferr_return;
      Int length = bytes.GetCount();
      if (length && bytes[length - 1] == 0) --length;
      names.push_back({std::string(), length ? std::string(bytes.GetFirst(), static_cast<std::size_t>(length)) : std::string()});
    }
    std::vector<wire_group_draw::MatchedPort> matched;
    if (!wire_group_draw::MatchInOrder(circles, names, matched) && !wire_group_draw::MatchSubsequence(circles, names, matched) &&
        !wire_group_draw::MatchSubset(circles, names, matched)) return false;
    maxon::BaseArray<maxon::GraphNode> shown;
    rows.Flush();
    for (const auto& entry : matched) {
      shown.Append(ports[Int(entry.port)]) iferr_return;
      rows.Append(toGraphY(entry.y) - n.pos.y - kHeader) iferr_return;
    }
    ports = std::move(shown);
    return true;
  };
  for (NodeGeom& n : nodes) {
    const wire_group_draw::DrawnNode* body = DrawnBody(canvas.drawn, view, n.pos, n.width);
    if (!body) continue;
    NodeGeom measured;
    measured.node = n.node;
    measured.pos = n.pos;
    measured.width = body->w / view.zoom;
    measured.outs.CopyFrom(n.outs) iferr_return;
    measured.ins.CopyFrom(n.ins) iferr_return;
    const Bool outs = match(measured, body->outs, measured.outs, measured.outY) iferr_return;
    const Bool ins = match(measured, body->ins, measured.ins, measured.inY) iferr_return;
    if (!outs || !ins) {
      static std::unordered_set<std::string> reported;
      const auto path = n.node.GetPath().ToString().GetCString() iferr_return;
      if (reported.insert(std::string(path.GetFirst())).second)
        DebugLog(FormatString("view: drawn ports of @ do not match its shown ports, using estimates", n.node.GetPath()));
      continue;
    }
    // The title row (24 at zoom 1) sits above the body.
    measured.top = toGraphY(body->y) - 24.0;
    measured.bottom = toGraphY(body->y + body->h);
    measured.drawn = true;
    n = std::move(measured);
  }
  return maxon::OK;
}
// Lets the user choose the target input from a popup at the cursor, like dropping a
// wire on a node body in the editor. Returns an invalid node when cancelled.
maxon::Result<maxon::GraphNode> PickInput(const maxon::GraphNode& source, const maxon::GraphNode& startPort,
  const maxon::GraphNode& target, const maxon::BaseArray<wire_ops::Rule>& rules) {
  iferr_scope;
  const wire_ops::PortPair automatic = wire_ops::ChoosePortPair(source, target, startPort, maxon::GraphNode(), rules, true) iferr_return;
  if (!automatic.out.IsValid()) return maxon::GraphNode();
  const maxon::GraphNode inputs = target.GetInputs() iferr_return;
  const maxon::BaseArray<maxon::GraphNode> all = wire_ops::EditorOrderPorts(inputs, maxon::NODE_KIND::INPORT) iferr_return;
  maxon::BaseArray<maxon::GraphNode> choices;
  BaseContainer menu;
  for (const maxon::GraphNode& in : all) {
    const Bool ok = automatic.out.IsConnectable(in) iferr_return;
    if (!ok || choices.GetCount() >= 99) continue;
    const maxon::String name = in.GetValue(maxon::EffectiveName).GetOrDefault() iferr_return;
    const Bool connected = Connected(in, maxon::PORT_DIR::INPUT) iferr_return;
    menu.SetString(FIRST_POPUP_ID + Int32(choices.GetCount()), connected ? name + " *"_s : name);
    choices.Append(in) iferr_return;
  }
  if (choices.GetCount() == 1) return choices[0];
  if (choices.IsEmpty()) return maxon::GraphNode();
  const Int32 picked = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, menu, POPUP_RIGHT | POPUP_ALLOW_FILTERING);
  const Int index = picked - FIRST_POPUP_ID;
  return index >= 0 && index < choices.GetCount() ? choices[index] : maxon::GraphNode();
}

maxon::Result<Bool> NodeHasWires(const maxon::GraphNode& node) {
  iferr_scope;
  Bool any = false;
  const maxon::GraphNode lists[] = {node.GetInputs().GetValue(), node.GetOutputs().GetValue()};
  for (Int i = 0; i < 2 && !any; ++i) {
    lists[i].GetChildren([&any, i](const maxon::GraphNode& p) -> maxon::Result<Bool> {
      iferr_scope;
      any = Connected(p, i == 0 ? maxon::PORT_DIR::INPUT : maxon::PORT_DIR::OUTPUT) iferr_return;
      return !any;
    }, i == 0 ? maxon::NODE_KIND::INPORT : maxon::NODE_KIND::OUTPORT) iferr_return;
  }
  return any;
}
maxon::Result<void> DoGrabNode() {
  iferr_scope;
  g.grabNode = maxon::GraphNode();
  const maxon::nodes::NodesGraphModelRef graph = wire_ops::ActiveGraph(GetActiveDocument()) iferr_return;
  const maxon::BaseArray<ViewSnapshot> views = ReadActiveViews() iferr_return;
  CanvasView canvas;
  ResolveView(g.dragCanvas, views, canvas) iferr_return;
  const ViewSnapshot* active = canvas.active;
  const wire_native::ViewRecord* view = active ? &active->view : nullptr;
  if (!view) return maxon::OK;
  POINT origin{0, 0};
  ClientToScreen(g.dragCanvas, &origin);
  const Vec p{(Float(g.dragStart.x - origin.x - kOriginDx) - view->offset.x) / view->zoom, (Float(g.dragStart.y - origin.y - kOriginDy) - view->offset.y) / view->zoom};
  maxon::BaseArray<NodeGeom> nodes = Layout(active->container) iferr_return;
  ApplyDrawn(nodes, canvas) iferr_return;
  const NodeGeom* hit = HitNode(nodes, p);
  if (!hit) return maxon::OK;
  g.grabNode = hit->node;
  g.grabX = hit->pos.x;
  g.grabY = hit->pos.y;
  return maxon::OK;
}
void GrabNode() {
  iferr (DoGrabNode()) { g.grabNode = maxon::GraphNode(); }
}
// The cursor rests while an unwired node is dragged: if the node lies across a wire,
// splice it in right away. Ports: the node's first free shown input and first shown
// output that fit, otherwise the usual rules.
maxon::Result<void> DoDropInsert() {
  iferr_scope;
  const maxon::nodes::NodesGraphModelRef graph = wire_ops::ActiveGraph(GetActiveDocument()) iferr_return;
  // The editor selects the node it drags wherever it was grabbed (title, thumbnail,
  // ports), so a single selected node wins over the button-down hit test.
  maxon::BaseArray<maxon::GraphNode> selected;
  maxon::GraphModelHelper::GetSelectedNodes(graph, maxon::NODE_KIND::NODE, [&selected](const maxon::GraphNode& n) -> maxon::Result<Bool> {
    iferr_scope;
    selected.Append(n) iferr_return;
    return true;
  }) iferr_return;
  const maxon::GraphNode grabbed = selected.GetCount() == 1 ? selected[0] : g.grabNode;
  if (!grabbed.IsValid()) { DebugLog("drop: drag did not start on a node"_s); return maxon::OK; }
  const Bool wired = NodeHasWires(grabbed) iferr_return;
  if (wired) { DebugLog("drop: dragged node already has wires"_s); return maxon::OK; }
  const maxon::BaseArray<ViewSnapshot> views = ReadActiveViews() iferr_return;
  CanvasView canvas;
  ResolveView(g.dragCanvas, views, canvas) iferr_return;
  const ViewSnapshot* active = canvas.active;
  const wire_native::ViewRecord* view = active ? &active->view : nullptr;
  if (!view) { DebugLog("drop: no view for canvas"_s); return maxon::OK; }
  maxon::BaseArray<NodeGeom> nodes = Layout(active->container) iferr_return;
  ApplyDrawn(nodes, canvas) iferr_return;
  const NodeGeom* dragged = nullptr;
  for (const NodeGeom& n : nodes) if (n.node == grabbed) dragged = &n;
  if (!dragged) { DebugLog("drop: dragged node has no layout record"_s); return maxon::OK; }
  // The layout record keeps the position from before the drag until the drop.
  g.grabX = dragged->pos.x;
  g.grabY = dragged->pos.y;
  POINT cursor;
  if (!GetCursorPos(&cursor)) return maxon::OK;
  const Float dx = Float(cursor.x - g.dragStart.x) / view->zoom, dy = Float(cursor.y - g.dragStart.y) / view->zoom;
  const Float left = g.grabX + dx, right = left + dragged->width;
  const Float top = g.grabY + dy - kTitleRow, bottom = g.grabY + dy + dragged->Height();
  maxon::BaseArray<Vec> box;
  box.Append(Vec{left, top}) iferr_return;
  box.Append(Vec{right, top}) iferr_return;
  box.Append(Vec{right, bottom}) iferr_return;
  box.Append(Vec{left, bottom}) iferr_return;
  box.Append(Vec{left, top}) iferr_return;
  const Vec center{(left + right) * 0.5, (top + bottom) * 0.5};
  const maxon::BaseArray<WireGeom> wires = Wires(nodes, BoundaryPorts{}) iferr_return;
  const maxon::BaseArray<wire_ops::Rule> rules = wire_ops::LoadRules() iferr_return;
  const WireGeom* best = nullptr;
  wire_ops::PortPair bestFirst, bestSecond;
  Float bestDistance = MAXVALUE_FLOAT64;
  for (const WireGeom& wire : wires) {
    if (!PolylinesCross(box, wire.curve)) continue;
    const Float distance = PolylineDistance(center, wire.curve);
    if (distance >= bestDistance) continue;
    wire_ops::PortPair first, second;
    for (const maxon::GraphNode& in : dragged->ins) {
      const Bool used = Connected(in, maxon::PORT_DIR::INPUT) iferr_return;
      if (used) continue;
      const Bool ok = wire.out.IsConnectable(in) iferr_return;
      if (ok) { first = wire_ops::PortPair{wire.out, in}; break; }
    }
    for (const maxon::GraphNode& out : dragged->outs) {
      const Bool ok = out.IsConnectable(wire.in) iferr_return;
      if (ok) { second = wire_ops::PortPair{out, wire.in}; break; }
    }
    const maxon::GraphNode source = wire.out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
    const maxon::GraphNode target = wire.in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
    if (!first.in.IsValid()) { first = wire_ops::ChoosePortPair(source, dragged->node, wire.out, maxon::GraphNode(), rules, true) iferr_return; }
    if (!second.out.IsValid()) { second = wire_ops::ChoosePortPair(dragged->node, target, maxon::GraphNode(), wire.in, rules, true) iferr_return; }
    if (!first.out.IsValid() || !first.in.IsValid() || !second.out.IsValid() || !second.in.IsValid()) continue;
    best = &wire;
    bestFirst = first;
    bestSecond = second;
    bestDistance = distance;
  }
  if (!best) return maxon::OK;
  maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
  maxon::GraphModelHelper::RemoveConnection(best->out, best->in) iferr_return;
  wire_ops::ConnectReplacing(bestFirst.out, bestFirst.in) iferr_return;
  wire_ops::ConnectReplacing(bestSecond.out, bestSecond.in) iferr_return;
  transaction.Commit() iferr_return;
  g.inserted = true;
  StatusSetText(GeLoadString(IDS_GESTURE_INSERTED));
  EventAdd();
  return maxon::OK;
}
void TryDropInsert() {
  iferr (DoDropInsert()) { StatusSetText(err.GetMessage()); }
}

// --- execution ---------------------------------------------------------------
maxon::Result<void> Execute(Mode mode, HWND canvas, const maxon::BaseArray<ScreenPoint>& screen) {
  iferr_scope;
  if (screen.IsEmpty() || ((mode == Mode::CONNECT || mode == Mode::CUT) && screen.GetCount() < 2)) return maxon::OK;
  const maxon::nodes::NodesGraphModelRef graph = wire_ops::ActiveGraph(GetActiveDocument()) iferr_return;
  const maxon::BaseArray<ViewSnapshot> views = ReadActiveViews() iferr_return;
  CanvasView shown;
  ResolveView(canvas, views, shown) iferr_return;
  const ViewSnapshot* active = shown.active;
  const wire_native::ViewRecord* view = active ? &active->view : nullptr;
  if (!view) {
    DebugLog(FormatString("gesture: no view for canvas, views=@", views.GetCount()));
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, shown.drawing.window
      ? GeLoadString(IDS_GESTURE_OTHER_GRAPH)
      : GeLoadString(IDS_GESTURE_UNKNOWN_EDITOR));
  }
  POINT origin{0, 0};
  ClientToScreen(canvas, &origin);
  maxon::BaseArray<Vec> stroke;
  for (const ScreenPoint& p : screen) {
    stroke.Append(Vec{(Float(p.x - origin.x - kOriginDx) - view->offset.x) / view->zoom,
                      (Float(p.y - origin.y - kOriginDy) - view->offset.y) / view->zoom}) iferr_return;
  }
  maxon::BaseArray<NodeGeom> nodes = Layout(active->container) iferr_return;
  ApplyDrawn(nodes, shown) iferr_return;
  const BoundaryPorts boundary = GroupBoundary(active->container, graph.GetViewRoot(), shown.drawing, *view) iferr_return;

  if (mode == Mode::SHAKE) {
    // Take the dragged node (and the rest of the selection if it is part of it) out of
    // its wires; upstream and downstream are joined where the types match.
    // The editor selects the node it drags, so the selection is what is being shaken.
    maxon::BaseArray<maxon::GraphNode> targets;
    maxon::GraphModelHelper::GetSelectedNodes(graph, maxon::NODE_KIND::NODE, [&targets](const maxon::GraphNode& n) -> maxon::Result<Bool> {
      iferr_scope;
      targets.Append(n) iferr_return;
      return true;
    }) iferr_return;
    if (targets.IsEmpty()) {
      const NodeGeom* hit = HitNode(nodes, stroke[0]);
      if (!hit) return maxon::OK;
      targets.Append(hit->node) iferr_return;
    }
    maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
    for (const maxon::GraphNode& n : targets) { wire_ops::ExtractNode(n) iferr_return; }
    transaction.Commit() iferr_return;
    StatusSetText(GeLoadString(IDS_GESTURE_EXTRACTED, String::IntToString(targets.GetCount())));
    return maxon::OK;
  }

  if (mode == Mode::CONNECT) {
    // The two group boundary ports have reversed roles inside the group: its
    // input is a source, its output is a target.
    const Float radius = 16.0 / view->zoom;
    Vec first = stroke[0], last = stroke[stroke.GetCount() - 1];
    // Interface dots sit on the edge between canvas and side panel, and strokes tend
    // to start or end beyond them, inside the panel. A point hits a dot when it lies on
    // the dot's row and anywhere from `radius` inside the canvas outwards; the nearest
    // row wins, and two rows about equally near are refused.
    auto closest = [radius](const auto& ports, Vec point, Bool left) -> const BoundaryPort* {
      const BoundaryPort* best = nullptr;
      Float bestDistance = radius * 1.25, second = 1e30;
      for (const auto& port : ports) {
        const Float inward = left ? point.x - port.pos.x : port.pos.x - point.x;
        if (inward > radius) continue;
        const Float distance = Abs(point.y - port.pos.y);
        if (distance < bestDistance) { if (best) second = bestDistance; best = &port; bestDistance = distance; }
        else if (distance < second) second = distance;
      }
      return best && second - bestDistance > radius * 0.125 ? best : nullptr;
    };
    const auto* fromBoundary = closest(boundary.sources, first, true);
    const auto* toBoundary = closest(boundary.targets, last, false);
    // A stroke may run either way: node to group input, or group output to node.
    if (!fromBoundary && !toBoundary) {
      fromBoundary = closest(boundary.sources, last, true);
      toBoundary = closest(boundary.targets, first, false);
      if (fromBoundary || toBoundary) std::swap(first, last);
    }
    if (fromBoundary || toBoundary) {
      maxon::GraphNode out = fromBoundary ? fromBoundary->port : maxon::GraphNode();
      maxon::GraphNode in = toBoundary ? toBoundary->port : maxon::GraphNode();
      if (!out.IsValid()) {
        const NodeGeom* source = HitNode(nodes, first);
        if (source) {
          out = PortAt(*source, first, true);
          if (!out.IsValid() && !source->outs.IsEmpty()) out = source->outs[0];
        }
      }
      if (!in.IsValid()) {
        const NodeGeom* target = HitNode(nodes, last);
        if (target) {
          in = PortAt(*target, last, false);
          if (!in.IsValid() && out.IsValid()) {
            for (const maxon::GraphNode& candidate : target->ins) {
              const Bool used = Connected(candidate, maxon::PORT_DIR::INPUT) iferr_return;
              const Bool fits = out.IsConnectable(candidate) iferr_return;
              if (!used && fits) { in = candidate; break; }
            }
          }
        }
      }
      if (!out.IsValid() || !in.IsValid())
        return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_GESTURE_GROUP_STROKE));
      const Bool allowed = out.IsConnectable(in) iferr_return;
      if (!allowed) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_GESTURE_NOT_CONNECTABLE));
      maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
      wire_ops::ConnectReplacing(out, in) iferr_return;
      transaction.Commit() iferr_return;
      StatusSetText(GeLoadString(IDS_GESTURE_GROUP_CONNECTED));
      return maxon::OK;
    }
    // Nodes in stroke order, with where the stroke entered the first node and left the last.
    struct Hit { const NodeGeom* node; Vec first, last; };
    maxon::BaseArray<Hit> hits;
    for (const Vec& p : stroke) {
      const NodeGeom* hit = HitNode(nodes, p);
      if (!hit) continue;
      if (!hits.IsEmpty() && hits[hits.GetCount() - 1].node == hit) { hits[hits.GetCount() - 1].last = p; continue; }
      hits.Append(Hit{hit, p, p}) iferr_return;
    }
    if (hits.GetCount() < 2) {
      StatusSetText(GeLoadString(IDS_GESTURE_TWO_NODES));
      return maxon::OK;
    }
    {
      // A UV Context Projection only feeds UV Context inputs: a stroke starting on it
      // wires it into the uv_context input of every node it crosses (e.g. all textures).
      const maxon::IdAndVersion asset = hits[0].node->node.GetValue(maxon::nodes::AssetId).GetOrDefault() iferr_return;
      if (asset.first.ToString().EndsWith(".uvcontextprojection"_s)) {
        const maxon::GraphNode outputs = hits[0].node->node.GetOutputs() iferr_return;
        const maxon::BaseArray<maxon::GraphNode> outs = wire_ops::EditorOrderPorts(outputs, maxon::NODE_KIND::OUTPORT) iferr_return;
        if (outs.IsEmpty()) return maxon::OK;
        maxon::BaseArray<wire_ops::PortPair> links;
        for (Int i = 1; i < hits.GetCount(); ++i) {
          const maxon::GraphNode inputs = hits[i].node->node.GetInputs() iferr_return;
          maxon::GraphNode context;
          inputs.GetChildren([&context](const maxon::GraphNode& p) -> maxon::Result<Bool> {
            if (p.GetId().ToString().EndsWith(".uv_context"_s)) context = p;
            return !context.IsValid();
          }, maxon::NODE_KIND::INPORT) iferr_return;
          if (!context.IsValid()) continue;
          const Bool ok = outs[0].IsConnectable(context) iferr_return;
          if (ok) { links.Append(wire_ops::PortPair{outs[0], context}) iferr_return; }
        }
        if (links.IsEmpty()) {
          StatusSetText(GeLoadString(IDS_GESTURE_UV_ONLY));
          return maxon::OK;
        }
        maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
        for (const wire_ops::PortPair& link : links) { wire_ops::ConnectReplacing(link.out, link.in) iferr_return; }
        transaction.Commit() iferr_return;
        StatusSetText(GeLoadString(IDS_GESTURE_UV_CONNECTED, String::IntToString(links.GetCount())));
        return maxon::OK;
      }
    }
    const maxon::BaseArray<wire_ops::Rule> rules = wire_ops::LoadRules() iferr_return;
    // The ports under the stroke's start and end are used exactly; others are chosen.
    const Hit& head = hits[0];
    const Hit& tail = hits[hits.GetCount() - 1];
    const maxon::GraphNode startPort = PortAt(*head.node, head.first, true);
    maxon::GraphNode endPort = PortAt(*tail.node, tail.last, false);
    // Decide every link before the transaction, since a popup menu may be needed.
    // Precedence: exact ports under the stroke > user rules > the first free shown
    // input (top to bottom, as the user reads the node) > Redshift workflow rules
    // > a menu of inputs (two-node strokes) or automatic choice (longer chains).
    struct Plan { wire_rs::Suggestion rs; wire_ops::PortPair exact; };
    maxon::BaseArray<Plan> plans;
    for (Int i = 1; i < hits.GetCount(); ++i) {
      const maxon::GraphNode& source = hits[i - 1].node->node;
      const maxon::GraphNode& target = hits[i].node->node;
      const maxon::GraphNode fixedOut = i == 1 ? startPort : maxon::GraphNode();
      maxon::GraphNode fixedIn = i == hits.GetCount() - 1 ? endPort : maxon::GraphNode();
      Plan plan;
      const Bool ruled = wire_ops::HasRule(source, target, rules) iferr_return;
      if (!fixedIn.IsValid() && !ruled) {
        // Not ended on a port row (e.g. on the title or thumbnail), or a node in the
        // middle of a chain: take the first shown input that is still free.
        const NodeGeom& from = *hits[i - 1].node;
        const maxon::GraphNode out = fixedOut.IsValid() ? fixedOut : (from.outs.IsEmpty() ? maxon::GraphNode() : from.outs[0]);
        if (out.IsValid()) {
          for (const maxon::GraphNode& in : hits[i].node->ins) {
            const Bool used = Connected(in, maxon::PORT_DIR::INPUT) iferr_return;
            if (used) continue;
            const Bool ok = out.IsConnectable(in) iferr_return;
            if (ok) { fixedIn = in; break; }
          }
          if (fixedIn.IsValid()) {
            plan.exact = wire_ops::PortPair{out, fixedIn};
            plans.Append(std::move(plan)) iferr_return;
            continue;
          }
        }
      }
      if (!fixedIn.IsValid() && !ruled) {
        plan.rs = wire_rs::Suggest(source, fixedOut, target) iferr_return;
        if (!plan.rs.IsValid() && hits.GetCount() == 2) {
          fixedIn = PickInput(source, fixedOut, target, rules) iferr_return;
          if (!fixedIn.IsValid()) return maxon::OK;  // menu cancelled or nothing fits
        }
      }
      if (!plan.rs.IsValid()) {
        plan.exact = wire_ops::ChoosePortPair(source, target, fixedOut, fixedIn, rules, true) iferr_return;
        if (!plan.exact.out.IsValid())
          return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_GESTURE_NOT_CONNECTABLE));
      }
      plans.Append(std::move(plan)) iferr_return;
    }
    maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
    for (const Plan& plan : plans) {
      if (plan.rs.IsValid()) { wire_rs::Apply(graph, plan.rs) iferr_return; }
      else {
        const wire_rs::Suggestion exact = wire_rs::Exact(plan.exact.out, plan.exact.in) iferr_return;
        wire_rs::Apply(graph, exact) iferr_return;
      }
    }
    transaction.Commit() iferr_return;
    StatusSetText(GeLoadString(IDS_GESTURE_CONNECTED, String::IntToString(hits.GetCount())));
    return maxon::OK;
  }

  struct Cut { maxon::GraphNode out, in; };
  maxon::BaseArray<Cut> cuts;
  const maxon::BaseArray<WireGeom> wires = Wires(nodes, boundary) iferr_return;
  const Float margin = kCutMargin / view->zoom;
  for (const WireGeom& wire : wires) {
    if (StrokeCutsWire(stroke, wire.curve, margin)) { cuts.Append(Cut{wire.out, wire.in}) iferr_return; }
  }
  if (cuts.IsEmpty()) {
    StatusSetText(GeLoadString(IDS_GESTURE_NO_WIRES));
    return maxon::OK;
  }
  maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
  for (const Cut& c : cuts) {
    maxon::GraphModelHelper::RemoveConnection(c.out, c.in) iferr_return;
  }
  transaction.Commit() iferr_return;
  StatusSetText(GeLoadString(IDS_GESTURE_CUT, String::IntToString(cuts.GetCount())));
  return maxon::OK;
}

class GestureMessage final : public MessageData {
public:
  Bool CoreMessage(Int32 id, const BaseContainer&) override {
    if (id != kGestureMessageId || g.pendingMode == Mode::NONE) return true;
    const Mode mode = g.pendingMode;
    g.pendingMode = Mode::NONE;
    if (!HasBoundCanvas(g.pendingCanvas) || !(g.pendingRoot == g.contextRoot)) {
      DebugLog("gesture: canvas unbound or graph changed"_s);
      g.pending.Flush();
      StatusSetText(GeLoadString(IDS_GESTURE_CHANGED));
      return true;
    }
    iferr (Execute(mode, g.pendingCanvas, g.pending)) {
      StatusSetText(err.GetMessage());
      ApplicationOutput("@", err);
      DebugLog("gesture: failed: "_s + err.GetMessage());
    }
    g.pending.Flush();
    EventAdd();
    return true;
  }
};
class ToggleCommand final : public CommandData {
public:
  // Flips the "Enable Gestures" preference, so a shortcut and the preference page agree.
  Bool Execute(BaseDocument*, GeDialog*) override {
    wire_prefs::SetEnabled(NODESTROKE_PREFS_GESTURES, !g.enabled);
    StatusSetText(g.enabled ? GeLoadString(IDS_GESTURE_ON) : GeLoadString(IDS_GESTURE_OFF));
    return true;
  }
  Int32 GetState(BaseDocument*, GeDialog*) override { return CMD_ENABLED | (g.enabled ? CMD_VALUE : 0); }
};
}

// Kept out of the Extensions menu; available in the Commander for a shortcut.
Bool RegisterCanvasGestures() {
  if (!RegisterMessagePlugin(kGestureMessageId, "NodeStroke Gestures"_s, 0, NewObjClear(GestureMessage))) return false;
  return RegisterCommandPlugin(kToggleId, GeLoadString(IDS_CMD_TOGGLE), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_TOGGLE_HELP), NewObjClear(ToggleCommand));
}
void ApplyGesturePrefs() {
  g.enabled = wire_prefs::Enabled(NODESTROKE_PREFS_GESTURES);
  g.connect = wire_prefs::Enabled(NODESTROKE_PREFS_CONNECT);
  g.cut = wire_prefs::Enabled(NODESTROKE_PREFS_CUT);
  g.shake = wire_prefs::Enabled(NODESTROKE_PREFS_SHAKE);
  g.drop = wire_prefs::Enabled(NODESTROKE_PREFS_DROP);
}
void StartCanvasGestures() {
  ApplyGesturePrefs();
  if (!g_gdiplus) {
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplus, &input, nullptr) != Gdiplus::Ok) g_gdiplus = 0;
  }
  if (!g.hook) g.hook = SetWindowsHookExW(WH_GETMESSAGE, GestureHook, nullptr, GetCurrentThreadId());
  if (!g.dragEvent) g.dragEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!g.watcher && g.dragEvent) { g.stopWatcher = false; g.watcher = CreateThread(nullptr, 0, WatchDwell, nullptr, 0, nullptr); }
}
void StopCanvasGestures() {
  if (g.watcher) {
    g.stopWatcher = true;
    SetEvent(g.dragEvent);
    WaitForSingleObject(g.watcher, 1000);
    CloseHandle(g.watcher);
    g.watcher = nullptr;
  }
  if (g.hook) UnhookWindowsHookEx(g.hook);
  g.hook = nullptr;
  if (g.overlay) DestroyWindow(g.overlay);
  g.overlay = nullptr;
  ReleaseSurface();
  if (g_gdiplus) Gdiplus::GdiplusShutdown(g_gdiplus);
  g_gdiplus = 0;
}
#else
Bool RegisterCanvasGestures() { return true; }
void ApplyGesturePrefs() {}
void StartCanvasGestures() {}
void StopCanvasGestures() {}
#endif
