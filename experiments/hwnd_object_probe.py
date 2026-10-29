"""Read-only C4D 2027.0 Windows canvas-object research probe.

Run on C4D's main thread inside the host. This is not plugin functionality.
The gui.module hash and offsets are specific to the inspected local build.
It uses bounded ReadProcessMemory calls, never changes memory or calls an
undocumented method, and prints only window/object IDs and class names.
"""
import ctypes
import hashlib
import os
import struct
from ctypes import wintypes
from pathlib import Path

import c4d

GUI_PATH = Path(r"C:\Program Files\Maxon Cinema 4D 2027\corelibs\gui.module.xdl64")
GUI_SHA256 = "2274410ec73d65612b39c29f50f37dc870e6d5aa8649c214159606e145d82217"
WNDPROC_RVA = 0x23190
TABLE_RVA = 0x12DE60
MASK_RVA = 0x12DE68

if c4d.GetC4DVersion() != 2027000 or hashlib.sha256(GUI_PATH.read_bytes()).hexdigest() != GUI_SHA256:
    raise RuntimeError("Unsupported C4D/gui.module build: refusing internal-memory probe")

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32.GetWindowLongPtrW.argtypes = (wintypes.HWND, ctypes.c_int)
user32.GetWindowLongPtrW.restype = ctypes.c_void_p
kernel32.GetCurrentProcess.restype = wintypes.HANDLE
kernel32.ReadProcessMemory.argtypes = (wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                      ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t))
kernel32.ReadProcessMemory.restype = wintypes.BOOL
kernel32.GetModuleHandleExW.argtypes = (wintypes.DWORD, ctypes.c_void_p, ctypes.POINTER(wintypes.HMODULE))
kernel32.GetModuleHandleExW.restype = wintypes.BOOL
kernel32.GetModuleFileNameW.argtypes = (wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD)

def read(address, size):
    if not address or size < 1 or size > 512:
        return None
    buf = ctypes.create_string_buffer(size)
    done = ctypes.c_size_t()
    ok = kernel32.ReadProcessMemory(kernel32.GetCurrentProcess(), address, buf, size, ctypes.byref(done))
    return buf.raw if ok and done.value == size else None

def qword(address):
    raw = read(address, 8)
    return struct.unpack("<Q", raw)[0] if raw else 0

def class_name(obj):
    info = qword(obj)
    header = read(info, 32)
    if not header:
        return ""
    _, _, name, length = struct.unpack("<4Q", header)
    if not 5 <= length <= 160:
        return ""
    raw = read(name, length)
    try:
        result = raw.decode("ascii") if raw else ""
    except UnicodeDecodeError:
        return ""
    return result if result.startswith("net.maxon.") else ""

def module_for(address):
    module = wintypes.HMODULE()
    if not address or not kernel32.GetModuleHandleExW(6, address, ctypes.byref(module)):
        return None, None
    name = ctypes.create_unicode_buffer(1024)
    kernel32.GetModuleFileNameW(module, name, len(name))
    return Path(name.value), module.value

def lookup(hwnd):
    procedure = user32.GetWindowLongPtrW(hwnd, -4)
    path, base = module_for(procedure)
    if not base or path.resolve() != GUI_PATH.resolve() or procedure - base != WNDPROC_RVA:
        return 0
    table, mask = qword(base + TABLE_RVA), qword(base + MASK_RVA)
    if not table or mask > 1_000_000:
        return 0
    value = hwnd ^ (hwnd >> 32)
    value ^= (value >> 12) ^ (value >> 20)
    value ^= (value >> 4) ^ (value >> 7)
    node = qword(table + 16 * (mask & value))
    matches = []
    for _ in range(64):
        if not node:
            break
        if qword(node + 8) == value:
            weak = qword(node + 24)
            obj = qword(weak)
            if obj and not obj & 1 and qword(node + 24) == weak and qword(weak) == obj:
                matches.append(obj)
        node = qword(node)
    if len(matches) != 1 or node:
        return 0
    return matches[0] if class_name(matches[0]) == "net.maxon.ui.oswindowclass.widgetuserdraw" else 0

CALLBACK = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.GetClassNameW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
user32.GetClassNameW.restype = ctypes.c_int
user32.IsWindowVisible.argtypes = (wintypes.HWND,)
user32.IsWindowVisible.restype = wintypes.BOOL
user32.EnumChildWindows.argtypes = (wintypes.HWND, CALLBACK, wintypes.LPARAM)
user32.EnumChildWindows.restype = wintypes.BOOL
user32.EnumWindows.argtypes = (CALLBACK, wintypes.LPARAM)
user32.EnumWindows.restype = wintypes.BOOL
windows = []
def collect(hwnd, _):
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    name = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, name, len(name))
    if pid.value == os.getpid() and user32.IsWindowVisible(hwnd) and name.value == "Static":
        windows.append(hwnd)
    return True

child_cb = CALLBACK(collect)
def top(hwnd, _):
    user32.EnumChildWindows(hwnd, child_cb, 0)
    return True

top_cb = CALLBACK(top)
user32.EnumWindows(top_cb, 0)
for hwnd in windows:
    root = lookup(hwnd)
    print("candidate hwnd", hex(hwnd), "object", hex(root), "class", class_name(root))
    if not root:
        continue
    queue = [(root, "root", 0)]
    seen = set()
    while queue and len(seen) < 250:
        obj, path, depth = queue.pop(0)
        if obj in seen:
            continue
        seen.add(obj)
        name = class_name(obj)
        if name and ("widgetclasses.app" in name or "canvasclasses.oswindow" in name):
            print("  linked", path, name)
        raw = read(obj, 512) if depth < 4 else None
        if not raw:
            continue
        for index, candidate in enumerate(struct.unpack("<64Q", raw)):
            if 0x10000000000 <= candidate < 0x7F0000000000 and candidate % 8 == 0 and candidate not in seen:
                if class_name(candidate):
                    queue.append((candidate, path + "/" + hex(index * 8), depth + 1))
