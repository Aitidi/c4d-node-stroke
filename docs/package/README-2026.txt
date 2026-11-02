NodeStroke — Cinema 4D 2026 compatibility preview (Windows x64)

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

兼容状态 / Compatibility status
这是适配预览版，已在 Cinema 4D 2026.4.0 中验证 I 连接、Y 切线、撤销，
以及自动创建 Bump/Output 节点后继续划线。
节点和端口位置按布局记录估算；精确端口定位和组接口端口尚不支持。
This is a compatibility preview, checked in Cinema 4D 2026.4.0 for I connections,
Y cuts, undo, and drawing again after automatic Bump/Output node creation.
Node and port positions are estimated from layout records; exact port positions
and group interface ports are not supported yet.

按住 I 移动鼠标连线，按住 Y 移动鼠标切线，松键执行。
设置：编辑 > 设置 > NodeStroke；命令：Commander 中搜索 NodeStroke。
Hold I and move to connect; hold Y and move to cut. Release the key to apply.
Settings: Edit > Preferences > NodeStroke. Search NodeStroke in the Commander.

Source and documentation: https://github.com/Aitidi/c4d-node-stroke
