NodeStroke — Cinema 4D 2026 compatibility preview (Windows x64)

Revision 7: keep the canvas binding across graph transactions.
修复自动创建 Bump 后 C4D 重建图模型、材质包装对象和视图根导致的绑定误清空。
The canvas binding follows the node-editor view key and transform when a graph
transaction replaces those wrappers.

使用 Cinema 4D 2026.0.0 C++ SDK 编译，API / ABI：2026000。
Built with the Cinema 4D 2026.0.0 C++ SDK, API / ABI 2026000.

安装 / Install
退出 C4D，把本文件所在文件夹整体复制到 C4D 2026 的用户 plugins 目录。
可在“编辑 > 设置 > 打开偏好设置文件夹”找到该目录。不要重复安装。
Quit C4D and copy this entire folder into the C4D 2026 user plugins folder.
Find it via Edit > Preferences > Open Preferences Folder. Install only one copy.

兼容状态 / Compatibility status
这是适配预览版。Cinema 4D 2026.4.0 宿主已加载插件，并完成用户实测的 I 连接、
Y 切线、Ctrl+Z 撤销，以及自动创建 Bump/Output 后无需滚动即可继续划线。
完整的多窗口、组切换和 DPI 组合仍需继续验收。
本机 c4dpy 在执行测试前崩溃；不加载本插件的对照运行也崩溃。
2026 的布局读取已放行，仍检查每条记录的数据类型与数值。
手势使用已有的校准降级路径：先选择目标材质，在节点编辑器画布上滚动或
用鼠标中键平移一次。视图变换无法匹配时可能需要重新校准。
2026 的精确绘制数据读取和组接口端口操作尚未适配，不启用 2027 内存偏移。

This is a compatibility preview. Cinema 4D 2026.4.0 has loaded the plugin, and
user checks passed for I connections, Y cuts, undo, and drawing immediately after
automatic Bump/Output creation without another canvas scroll.
The full multi-window, group-navigation and DPI matrix still needs validation.
Local c4dpy crashed before the test, including a baseline without this plugin.
The typed layout reader accepts 2026 and still validates every record.
Gestures use the existing calibrated fallback: select the target material and
scroll or middle-drag its node canvas first. Recalibrate when the view transform
cannot be matched.
Exact drawlist positions and group interface ports are not supported on 2026;
the 2027 raw-memory offsets remain disabled.

按住 I 移动鼠标连线，按住 Y 移动鼠标切线，松键执行。
设置：编辑 > 设置 > NodeStroke；命令：Commander 中搜索 NodeStroke。
Hold I and move to connect; hold Y and move to cut. Release the key to apply.
Settings: Edit > Preferences > NodeStroke. Search NodeStroke in the Commander.

Source and documentation: https://github.com/Aitidi/c4d-node-stroke
