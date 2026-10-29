"""Read live group-boundary circle candidates from C4D 2027.0 draw commands.

Usage: python experiments/drawlist_group_probe.py PID NODEGRAPH_ADDRESS
The root address is specific to the current process. This probe performs no
input, screenshots, memory writes, graph operations, or undocumented calls.
The candidates are observations, not yet a safe port-to-GraphNode mapping.
"""
import ctypes
import hashlib
import math
import struct
import sys
from ctypes import wintypes
from pathlib import Path

CORE = Path(r"C:\Program Files\Maxon Cinema 4D 2027\corelibs")
HASHES = {
    "c4d_nodeeditor.xdl64": "4528e0fd90c841cbb79014c1af4773380aebb59afdbda988b950804a242ff4cf",
    "gui_gluon.module.xdl64": "11b4611c4b589f7bd08d36575d24cb825c2e376049bb9aee141c6a2344427122",
}
if len(sys.argv) != 3:
    raise SystemExit(__doc__)
for filename, expected in HASHES.items():
    if hashlib.sha256((CORE / filename).read_bytes()).hexdigest() != expected:
        raise SystemExit("Unsupported build: " + filename)
pid, graph_window = int(sys.argv[1]), int(sys.argv[2], 0)
k = ctypes.WinDLL("kernel32", use_last_error=True)
k.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
k.OpenProcess.restype = wintypes.HANDLE
k.ReadProcessMemory.argtypes = (wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t))
k.ReadProcessMemory.restype = wintypes.BOOL
k.CloseHandle.argtypes = (wintypes.HANDLE,)
handle = k.OpenProcess(0x410, False, pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(address, size):
    if not address or not 0 < size <= 256:
        return b""
    buf, done = ctypes.create_string_buffer(size), ctypes.c_size_t()
    if not k.ReadProcessMemory(handle, address, buf, size, ctypes.byref(done)) or done.value != size:
        return b""
    return buf.raw

def qword(address):
    data = read(address, 8)
    return struct.unpack("<Q", data)[0] if data else 0

def class_name(address):
    data = read(qword(address), 32)
    if not data:
        return ""
    _, _, address, length = struct.unpack("<4Q", data)
    if not 1 <= length <= 160:
        return ""
    try:
        return read(address, length).decode("ascii")
    except UnicodeDecodeError:
        return ""

def draw_text(command):
    """Read this build's small drawing-string record, without invoking C4D."""
    text = struct.unpack_from("<Q", command, 48)[0]
    header = read(text, 48)
    if not header:
        return None
    encoding = header[0]
    pointer, length = struct.unpack_from("<2Q", header, 24)
    if not 0 < length <= 256:
        return None
    payload = read(pointer, length)
    if len(payload) != length:
        return None
    try:
        if encoding == 0 and length % 2 == 0:
            return payload.decode("utf-16-le")
        if encoding in (0xfe, 0xff):
            return payload.decode("utf-8")
    except UnicodeDecodeError:
        pass
    return None

try:
    if class_name(graph_window) != "net.maxon.mvp.gluon2.windowclasses.nodegraph":
        raise SystemExit("Invalid nodegraph window")
    drawlist = qword(graph_window + 0x58)
    app = qword(graph_window + 0x548)
    if class_name(drawlist) != "net.maxon.mvp.gluon2.drawlist" or class_name(app) != "net.maxon.mvp.presenterclasses.app":
        raise SystemExit("Unexpected window links")
    model = qword(app + 0xd0)
    if class_name(model) != "net.maxon.mvp.presenterclasses.graphmodel":
        raise SystemExit("Unexpected presenter model")
    component = drawlist + 0xe0
    chunks, chunk_count, fixed = qword(component + 0x30), qword(component + 0x48), qword(component + 0x58)
    if fixed != 1 or chunk_count != 1:
        raise SystemExit("Unsupported drawlist chunk layout")
    chunk = read(chunks, 32)
    if not chunk:
        raise SystemExit("Cannot read drawlist chunk")
    _, commands, count, capacity = struct.unpack("<4Q", chunk)
    if not 1 <= count <= capacity <= 1024:
        raise SystemExit("Invalid command count")
    data = [read(commands + i * 64, 64) for i in range(count)]
    if any(len(command) != 64 for command in data):
        raise SystemExit("Incomplete drawlist read")
    full = struct.unpack_from("<4f", data[0], 8)
    if data[0][0] != 1 or full[0] != 0 or full[1] != 0 or not (100 < full[2] < 16000 and 100 < full[3] < 16000):
        raise SystemExit("Unsupported canvas background")
    width, height = full[2:]
    panels = []
    for index, command in enumerate(data):
        if command[0] != 1:
            continue
        x, y, w, h = struct.unpack_from("<4f", command, 8)
        if not all(math.isfinite(v) for v in (x, y, w, h)):
            continue
        if abs(y) < 0.01 and abs(h - height) < 0.01 and 0 < w < width * 0.4:
            if abs(x) < 0.01:
                panels.append((index, "left", x + w))
            elif abs(x + w - width) < 0.01:
                panels.append((index, "right", x))
    if len(panels) != 2 or {panel[1] for panel in panels} != {"left", "right"}:
        raise SystemExit("Ambiguous or missing group side panels")
    print("window", hex(graph_window), "model", hex(model), "canvas", width, height)
    for n, (start, side, edge) in enumerate(panels):
        end = panels[n + 1][0] if n + 1 < len(panels) else len(data)
        circles = []
        current = None
        previous_radius = 0
        for command in data[start + 1:end]:
            if command[0] == 8 and current is not None:
                label = draw_text(command)
                if not label or current[2] is not None:
                    raise SystemExit("Ambiguous boundary label on " + side)
                current[2] = label
                continue
            if command[0] != 3:
                continue
            x, y, rx, ry = struct.unpack_from("<4f", command, 8)
            if all(math.isfinite(v) for v in (x, y, rx, ry)) and abs(x - edge) < 0.01 and 0 <= y <= height and 1 <= rx <= 20 and abs(rx - ry) < 0.01:
                if current is not None and current[2] is None and abs(y - current[1]) < 0.01 and rx < previous_radius:
                    previous_radius = rx
                    continue
                current = [round(x, 2), round(y, 2), None]
                circles.append(current)
                previous_radius = rx
        if not circles or len(circles) > 128 or any(c[2] is None for c in circles) or any(circles[i][1] >= circles[i + 1][1] for i in range(len(circles) - 1)):
            raise SystemExit("Ambiguous boundary circles on " + side)
        print(side, circles)
finally:
    k.CloseHandle(handle)
