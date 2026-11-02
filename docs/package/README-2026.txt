NodeStroke for Cinema 4D 2026 (Windows x64)

使用 Cinema 4D 2026.0.0 C++ SDK 编译，API / ABI：2026000。
Built with the Cinema 4D 2026.0.0 C++ SDK, API / ABI 2026000.

安装 / Install
退出 C4D，把本文件所在文件夹整体复制到 C4D 2026 的用户 plugins 目录。
可在“编辑 > 设置 > 打开偏好设置文件夹”找到该目录。不要重复安装。
Quit C4D and copy this entire folder into the C4D 2026 user plugins folder.
Find it via Edit > Preferences > Open Preferences Folder. Install only one copy.

使用前先校准 / Calibrate first
先选择目标材质，在节点编辑器画布上滚动或用鼠标中键平移一次。
按键划线没有反应时，再滚动一次即可。
Select the target material, then scroll or middle-drag its node canvas once.
If holding I or Y draws nothing, scroll the canvas again.

2026.4.0 之前的版本 / Before 2026.4.0
按布局记录估算节点和端口位置，连到组接口端口（组视图左右两侧）的连线
无法连接或切断。2026.4.0 上与 2027 版功能一致。
Node and port positions are estimated from layout records, and wires to a
group's interface ports (the left and right edges of the group view) cannot be
connected or cut. On 2026.4.0 everything works as in the 2027 release.

按住 I 移动鼠标连线，按住 Y 移动鼠标切线，松键执行。
扩展 > NodeStroke：开关手势、打开设置。更多命令：Commander 中搜索 NodeStroke。
Hold I and move to connect; hold Y and move to cut. Release the key to apply.
Extensions > NodeStroke: turn gestures on or off, open the settings.
More commands: search NodeStroke in the Commander.

Source and documentation: https://github.com/Aitidi/c4d-node-stroke
