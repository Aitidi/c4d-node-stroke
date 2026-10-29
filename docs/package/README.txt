NodeStroke for Cinema 4D 2027 (Windows)
=======================================

快速连接、切断和整理节点编辑器中的连线。
Draw, cut and tidy wires in the Cinema 4D node editor with the keyboard.


安装 / Installation
-------------------
1. 关闭 Cinema 4D。
   Quit Cinema 4D.
2. 把整个 NodeStroke 文件夹（含 nodestroke.xdl64 和 res 文件夹）复制到插件目录，例如：
   Copy the whole NodeStroke folder (nodestroke.xdl64 and the res folder) into a plugins folder, e.g.
   %APPDATA%\Maxon\Maxon Cinema 4D 2027_<id>\plugins\
   （编辑 > 设置 > 左下角“打开偏好设置文件夹”可找到该目录。
    Edit > Preferences > "Open Preferences Folder" shows where it is.)
3. 启动 Cinema 4D。
   Start Cinema 4D.

卸载：删除 NodeStroke 文件夹。
Uninstall: delete the NodeStroke folder.


使用 / Usage
------------
在节点编辑器中，鼠标放在画布上：
With the mouse over the node editor canvas:

- 按住 I 并移动鼠标：划过的节点或端口按顺序连接。从端口划到端口可精确指定端口。
  Hold I and move: connects the nodes or ports you pass, in order. Port to port picks exact ports.
- 按住 Y 并移动鼠标：切断划过的连线。
  Hold Y and move: cuts the wires you cross.
- 拖动节点并左右快速摇晃：把节点从连线中摘出，上下游自动接上。
  Drag a node and shake it left and right: takes it out of its wires and joins upstream to downstream.
- 把没有连线的节点拖到一根连线上停一下：插入到这根线中。
  Drag an unconnected node onto a wire and rest briefly: inserts it into the wire.

松开按键时执行；所有操作都可以用 Ctrl+Z 撤销。
Strokes apply when the key is released; every change can be undone with Ctrl+Z.

连到材质的凹凸输入或 Output 的置换输入时，会自动添加 Bump Map / Displacement 节点。
Connecting to a material's bump input or the Output's displacement input adds a Bump Map /
Displacement node automatically.


设置 / Settings
---------------
编辑 > 设置 > NodeStroke：可分别开关各手势、编辑连接规则、开启诊断日志（默认关闭）。
Edit > Preferences > NodeStroke: turn single gestures on or off, edit the connection rules,
turn on the diagnostic log (off by default).

可选：在“自定义命令”中搜索 NodeStroke，为“连接所选端口”“断开所选节点”等命令绑定快捷键。
Optional: search "NodeStroke" in Customize Commands to give commands such as
"Connect Selected Ports" or "Disconnect Selected Nodes" a shortcut.

插件界面（设置页、规则窗口、命令名称、状态栏提示和错误提示）跟随 Cinema 4D 的界面语言，支持简体中文和英文。
The plugin's interface follows Cinema 4D's language: Simplified Chinese or English.


兼容性 / Compatibility
----------------------
Windows，Cinema 4D 2027.0。组接口的连线操作与精确的节点/端口定位依赖该版本；
在其他版本中这些功能会自动停用，此时请先在节点编辑器中滚动一次画布再使用手势。
Windows, Cinema 4D 2027.0. Group interface wires and exact node/port positions depend on this
build; with other builds they turn themselves off, and the canvas must be scrolled once before
gestures work.


完整使用文档 / User guide
------------------------
中文：https://github.com/Aitidi/c4d-node-stroke/blob/main/docs/usage.zh-CN.md
English: https://github.com/Aitidi/c4d-node-stroke/blob/main/docs/usage.md
