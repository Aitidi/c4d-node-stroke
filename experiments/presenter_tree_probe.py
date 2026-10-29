"""Read-only, build-specific graph presenter relationship probe.

Usage: python experiments/presenter_tree_probe.py PID GRAPHMODEL_ADDRESS
Addresses must come from the current process. The observed relations do not
establish which editor or graph is active, and may change with any C4D build.
"""
import collections
import ctypes
import hashlib
import struct
import sys
from ctypes import wintypes
from pathlib import Path

BINARY = Path(r"C:\Program Files\Maxon Cinema 4D 2027\corelibs\c4d_nodeeditor.xdl64")
# Refuse unsupported binaries rather than generalizing observed layout offsets.
EXPECTED_HASH = "4528e0fd90c841cbb79014c1af4773380aebb59afdbda988b950804a242ff4cf"
if len(sys.argv) != 3:
    raise SystemExit(__doc__)
if hashlib.sha256(BINARY.read_bytes()).hexdigest() != EXPECTED_HASH:
    raise SystemExit("Unsupported node editor build")
pid, root = int(sys.argv[1]), int(sys.argv[2], 0)
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.ReadProcessMemory.argtypes = (wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t))
kernel.ReadProcessMemory.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
handle = kernel.OpenProcess(0x410, False, pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(address, size):
    if not address or not 0 < size <= 64:
        return b""
    buffer, done = ctypes.create_string_buffer(size), ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(done)) or done.value != size:
        return b""
    return buffer.raw

def qword(address):
    value = read(address, 8)
    return struct.unpack("<Q", value)[0] if value else 0

def name(address):
    header = read(qword(address), 32)
    if not header:
        return ""
    _, _, text, size = struct.unpack("<4Q", header)
    if not 5 <= size <= 64:
        return ""
    try:
        result = read(text, size).decode("ascii")
        return result if result.startswith("net.maxon.mvp.presenterclasses.") else ""
    except UnicodeDecodeError:
        return ""

try:
    if name(root) != "net.maxon.mvp.presenterclasses.graphmodel":
        raise SystemExit("Graph model class mismatch")
    queue, visited = collections.deque([root]), set()
    while queue and len(visited) < 100:
        obj = queue.popleft()
        if obj in visited:
            continue
        visited.add(obj)
        cls = name(obj)
        if not cls:
            continue
        links = []
        for offset in (0xb8, 0xc0, 0xc8, 0xd0, 0xd8):
            target = qword(obj + offset)
            target_class = name(target)
            if target != obj and target_class:
                links.append((hex(offset), target_class.rsplit(".", 1)[-1], hex(target)))
                queue.append(target)
        print(hex(obj), cls.rsplit(".", 1)[-1], links)
    print("visited", len(visited), "pending", len(queue))
finally:
    kernel.CloseHandle(handle)
