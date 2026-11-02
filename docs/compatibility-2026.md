# Cinema 4D 2026 Compatibility (Preview)

English | [简体中文](compatibility-2026.zh-CN.md)

NodeStroke can be built for Cinema 4D 2026 with the 2026.0.0 C++ SDK. The 2026 build is a compatibility preview: the core gestures work, but some features of the 2027 release are not available.

## What works and what does not

| Feature | 2027.0 | 2026 preview |
| --- | --- | --- |
| I connect, Y cut, undo | Yes | Yes |
| Automatic Bump / Displacement / Output nodes | Yes | Yes |
| Exact node and port positions from drawing data | Yes | No: estimated from the layout records |
| Gestures without calibrating the canvas first | Yes | No: scroll or middle-drag the canvas once |
| Wires between nodes inside a group | Yes | Yes |
| Wires to the group's interface ports (the left and right edges of the group view) | Yes | No: they cannot be connected or cut by drawing |

On 2026 the plugin reads node and view layout through typed `GraphNode` attributes only. Every dictionary, tuple and numeric value is still checked, and a read that does not match is refused. The 2027 raw-memory offsets and module hashes in `group_native_access.h` are unchanged and stay disabled on 2026.

A canvas calibration on 2026 survives graph transactions (for example automatic Bump node creation) and clicks outside the canvas. After a transaction it follows the node editor view with the same key, zoom and offset; if no single view matches, scroll the canvas again. Double-clicking into a group always clears it. 2027 keeps its original rules.

## Building

Unpack the Cinema 4D 2026.0.0 C++ SDK to `sdk-2026.0.0/` in the repository root (not tracked by Git), then run:

```powershell
Expand-Archive -LiteralPath .\Cinema_4D_CPP_SDK_2026_0_0.zip -DestinationPath .\sdk-2026.0.0
.\tools\build-2026.ps1
```

The script checks that the SDK's API and ABI version are both `2026000` and refuses newer SDKs, such as the one shipped with a 2026.4 installation. It builds into `build-c4d-2026.0.0/` and never reuses another SDK's cache. Visual Studio 2022 is the default; pass `-Generator` and `-PythonExecutable` to use another toolchain.

The 2026 SDK finds plugins through `projectdefinition.txt`. The plugin's `CMakeLists.txt` lets the SDK's own generator translate that file and returns early; the 2027 SDK ignores `projectdefinition.txt` and uses the CMake settings below that block. Keep the dependency list and module ID in both files in sync.

## Packaging

```powershell
.\tools\package.ps1 -BuildDirectory build-c4d-2026.0.0 -PackageName NodeStroke-C4D2026-preview -Readme docs/package/README-2026.txt
```

Output: `dist/NodeStroke-C4D2026-preview.zip`.

## Testing

- Model tests: `ctest --test-dir build -C Debug --output-on-failure`
- Group port label tests: `py -3 -m unittest discover -s tests -p test_*.py`
- Host registration: run `tests/host_smoke.py` in Cinema 4D with the plugin loaded; all 11 commands must be present.

Checked in the Cinema 4D 2026.4.0 GUI: loading, I connect, Y cut, undo, drawing again right after an automatic Bump or Output node was created without scrolling, and connecting and cutting wires between nodes inside a group after recalibrating there.

Still to check before a non-preview release:

- Multiple node editor windows
- Shake to disconnect and drop onto a wire
- 100% and 150% display scaling
- A build with the Visual Studio 2022 (v143) toolchain

## Diagnostics

Turn on logging in Edit > Preferences > NodeStroke. The log is written to `nodestroke_debug.txt` in the preferences folder and records canvas binding changes and each gesture's hits, planned links and commits.
