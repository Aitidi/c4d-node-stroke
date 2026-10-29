#include "c4d.h"
#if defined(MAXON_TARGET_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
using namespace cinema;

namespace {
struct WindowScan { DWORD process; };
HHOOK messageHook = nullptr;
int mouseEvents = 0;
thread_local bool inHook = false;

LRESULT CALLBACK ObserveMessages(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION && !inHook && lParam && mouseEvents < 32) {
    const MSG* msg = reinterpret_cast<const MSG*>(lParam);
    if (msg->message == WM_LBUTTONDOWN || msg->message == WM_LBUTTONUP) {
      inHook = true;
      char name[256] = {};
      GetClassNameA(msg->hwnd, name, sizeof(name));
      ApplicationOutput("NodeStroke input observation: message=@ target=@ class=@ screen=(@,@)"_s,
        (maxon::Int32)msg->message, (maxon::UInt64)(uintptr_t)msg->hwnd,
        maxon::String(name), msg->pt.x, msg->pt.y);
      ++mouseEvents;
      inHook = false;
    }
  }
  return CallNextHookEx(messageHook, code, wParam, lParam);
}

BOOL CALLBACK InspectChild(HWND window, LPARAM) {
  char className[256] = {};
  RECT rect = {};
  GetClassNameA(window, className, sizeof(className));
  if (!GetWindowRect(window, &rect)) return TRUE;
  ApplicationOutput("NodeStroke window child: handle=@ class=@ visible=@ rect=(@,@,@,@)"_s,
    (maxon::UInt64)(uintptr_t)window, maxon::String(className), IsWindowVisible(window) != FALSE,
    rect.left, rect.top, rect.right, rect.bottom);
  return TRUE;
}
BOOL CALLBACK InspectTop(HWND window, LPARAM param) {
  auto* scan = reinterpret_cast<WindowScan*>(param);
  DWORD process = 0;
  const DWORD thread = GetWindowThreadProcessId(window, &process);
  if (process != scan->process || !IsWindowVisible(window)) return TRUE;
  char className[256] = {};
  RECT rect = {};
  GetClassNameA(window, className, sizeof(className));
  if (!GetWindowRect(window, &rect)) return TRUE;
  ApplicationOutput("NodeStroke window top: handle=@ class=@ thread=@ rect=(@,@,@,@)"_s,
    (maxon::UInt64)(uintptr_t)window, maxon::String(className), thread,
    rect.left, rect.top, rect.right, rect.bottom);
  EnumChildWindows(window, InspectChild, 0);
  return TRUE;
}
}
void InspectNativeWindows() {
  WindowScan scan{GetCurrentProcessId()};
  EnumWindows(InspectTop, reinterpret_cast<LPARAM>(&scan));
}
bool ToggleNativeInputProbe() {
  if (messageHook) {
    UnhookWindowsHookEx(messageHook);
    messageHook = nullptr;
    return false;
  }
  mouseEvents = 0;
  messageHook = SetWindowsHookExW(WH_GETMESSAGE, ObserveMessages, nullptr, GetCurrentThreadId());
  return messageHook != nullptr;
}
void StopNativeInputProbe() {
  if (messageHook) UnhookWindowsHookEx(messageHook);
  messageHook = nullptr;
}
#else
void InspectNativeWindows() {}
bool ToggleNativeInputProbe() { return false; }
void StopNativeInputProbe() {}
#endif
