# NodeStroke for Cinema 4D 2027

[English](README.md) | 简体中文

NodeStroke 让你在 Cinema 4D 节点编辑器里用键盘加划线来连接、切断和整理连线。它是 C++ SDK 插件，支持 Windows 上的 Cinema 4D 2027.0（Redshift 节点材质）。

下载：见 [Releases](../../releases)。安装方法见压缩包里的 `README.txt`。

## 功能

鼠标放在节点编辑器画布上：

- **按住 I 并移动鼠标**：划过的节点或端口按顺序连接；从端口划到端口可精确指定端口。
- **按住 Y 并移动鼠标**：切断划过的连线。
- **拖动节点并左右快速摇晃**：把节点从连线中摘出，上下游自动接上。
- **把没有连线的节点拖到一根连线上停一下**：插入到这根线中。

松开按键时执行，所有操作都能用 Ctrl+Z 撤销。支持节点组和组接口端口。连到材质的凹凸输入或 Output 的置换输入时，会自动添加 Bump Map / Displacement 节点。

## 设置

**编辑 > 设置 > NodeStroke**：分别开关各手势、编辑连接规则、开启诊断日志（默认关闭；开启后手势未生效的原因写入偏好目录的 `nodestroke_debug.txt`）。设置随 C4D 偏好保存。界面文字跟随 C4D 界面语言（简体中文 / 英文）。

## 命令

以下命令不出现在扩展菜单中，可在命令管理器（Commander）里搜索，或在自定义命令里绑定快捷键。使用前先在材质管理器中选中目标节点材质，并在节点图中选中相应元素。

| 命令 | 用法 |
| --- | --- |
| `NodeStroke: Disconnect Selected Nodes` | 断开所选节点的所有连接 |
| `NodeStroke: Cut Selected Wires` | 删除所选连线 |
| `NodeStroke: Extract Selected Nodes` | 把所选节点从连线中取出，上游直接接到下游 |
| `NodeStroke: Connect Selected Ports` | 选一个输出口和一个输入口后连接；输入口已有连线则替换 |
| `NodeStroke: Chain Connect Selected Nodes` | 所选节点按画布从左到右依次相连，自动挑选端口 |
| `NodeStroke: Fan Out From Leftmost Node` | 最左节点连到其余每个节点 |
| `NodeStroke: Fan In To Rightmost Node` | 其余节点依次接入最右节点的空闲输入 |
| `NodeStroke: Insert Node Into Selected Wire` | 选一条连线和一个节点；或选连线两端的节点加要插入的节点 |
| `NodeStroke: Add Rule From Selected Ports` | 选一对端口：连接并记为这两种节点之间的默认端口 |
| `NodeStroke: Edit Connection Rules` | 规则窗口：查看、从所选端口添加、删除规则（规则文件 `<偏好目录>/nodestroke_rules.txt`）|
| `NodeStroke: Toggle Canvas Gestures` | 与设置页的“启用手势”是同一个开关 |

自动挑选端口的顺序：自定义规则 → 同类型族（如 col3/col4）的空闲输入 → 任意可连接的空闲输入 → 替换已占用输入（扇入不替换）。端口按节点编辑器中的显示顺序尝试。所有修改在一个图事务中完成，可撤销。

## 实现说明

节点编辑器没有公开画布坐标的 API。画布手势读取节点编辑器自身的视图记录与绘制数据（只读）来确定节点和端口位置，这部分按 C4D 2027.0 调校，并用模块 SHA-256 校验版本；版本不匹配时精确定位与组接口功能自动停用，需先在画布上滚动一次再使用手势。调查记录见 `docs/investigation.md`。

插件 ID 1069982–1069999 目前是未经 Maxon 注册的本机 ID，可能与其他插件冲突。

## 构建

需要 Cinema 4D 2027.0.0 C++ SDK（解压到仓库根目录的 `sdk/`，不纳入 Git）、CMake、Visual Studio 2022 和 Windows SDK：

```powershell
cmake -S sdk -B build-c4d -G "Visual Studio 17 2022" -A x64 -DMAXON_SDK_MODULES_DIR=<仓库路径>/c4d_plugin
cmake --build build-c4d --config Release --target nodestroke --parallel 8
```

产物：`build-c4d/bin/Release/plugins/nodestroke/nodestroke.xdl64`。把它和同目录的 `res` 文件夹一起复制到 C4D 2027 用户插件目录下的 `NodeStroke` 文件夹，重启 C4D 后加载。C4D 运行时 DLL 被锁定，可以先把旧文件改名再复制新文件。

打包：`powershell -ExecutionPolicy Bypass -File tools/package.ps1` 生成 `dist/NodeStroke/` 和 `dist/NodeStroke.zip`（含面向用户的 `README.txt`，来源 `docs/package/README.txt`）。设置页描述与全部界面字符串由 `python tools/make_res.py` 生成到 `c4d_plugin/nodestroke/res`。

独立模型测试（`src/`、`tests/`，不依赖 SDK）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`experiments/` 中是研发用的只读探针，不参与插件构建。隐藏的诊断命令 `NodeStroke: Inspect View Records`、`NodeStroke: Toggle Input Probe` 仅用于本机调查。
