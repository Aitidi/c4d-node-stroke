"""Bounded, read-only search from a user-supplied live C4D object.

Example: python experiments/scan_mvp_objects.py PID ROOT_ADDRESS EXPECTED_CLASS
Addresses are session-specific. Reachability through arbitrary memory words is
only a lead: it does not establish object ownership, identity or liveness.
No methods are invoked and no process memory is modified.
"""
import collections
import ctypes
import struct
import sys
import time
from ctypes import wintypes

if len(sys.argv) != 4:
    raise SystemExit(__doc__)
pid, root, expected = int(sys.argv[1]), int(sys.argv[2], 0), sys.argv[3]
MAX_NODES, MAX_DEPTH, MAX_SECONDS = 80000, 6, 10.0
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.ReadProcessMemory.argtypes = (wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t))
kernel.ReadProcessMemory.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
handle = kernel.OpenProcess(0x410, False, pid)  # query and read only
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(address, size):
    if not address or not 0 < size <= 512:
        return b""
    buffer, done = ctypes.create_string_buffer(size), ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(done)) or done.value != size:
        return b""
    return buffer.raw

def class_name(address):
    data = read(address, 8)
    if not data:
        return ""
    data = read(struct.unpack("<Q", data)[0], 32)
    if not data:
        return ""
    _, _, name, length = struct.unpack("<4Q", data)
    if not 5 <= length <= 160:
        return ""
    try:
        value = read(name, length).decode("ascii")
        return value if value.startswith("net.maxon.") else ""
    except UnicodeDecodeError:
        return ""

try:
    if class_name(root) != expected or not expected.startswith("net.maxon.mvp."):
        raise SystemExit("Root class mismatch; refusing scan")
    queue = collections.deque([(root, "root", 0)])
    seen, found = set(), {}
    start = time.monotonic()
    while queue and len(seen) < MAX_NODES and time.monotonic() - start < MAX_SECONDS:
        address, path, depth = queue.popleft()
        if address in seen:
            continue
        seen.add(address)
        name = class_name(address)
        if name.startswith("net.maxon.mvp.") and ("presenterclasses." in name or "windowclasses.nodegraph" in name):
            found.setdefault(name, (path, address))
        if depth >= MAX_DEPTH:
            continue
        data = read(address, 512)
        if not data:
            continue
        for index, value in enumerate(struct.unpack("<64Q", data)):
            if index == 0 and name:
                continue  # avoid following the class registry from known objects
            if 0x10000000000 <= value < 0x7F0000000000 and value % 8 == 0 and value not in seen:
                queue.append((value, path + "/" + hex(index * 8), depth + 1))
    print("visited", len(seen), "pending", len(queue), "elapsed", round(time.monotonic() - start, 2))
    for name, (path, address) in sorted(found.items()):
        print(name, path, hex(address))
finally:
    kernel.CloseHandle(handle)
