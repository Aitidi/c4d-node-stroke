# Cinema 4D 2026 适配（预览）

[English](compatibility-2026.md) | 简体中文

NodeStroke 可以用 2026.0.0 C++ SDK 构建出 Cinema 4D 2026 版本。2026 版是适配预览：核心手势可用，但 2027 版的部分功能尚不支持。

## 功能对照

| 功能 | 2027.0 | 2026 预览 |
| --- | --- | --- |
| I 连接、Y 切断、撤销 | 支持 | 支持 |
| 自动创建 Bump / Displacement / Output 节点 | 支持 | 支持 |
| 从绘制数据读取精确的节点和端口位置 | 支持 | 不支持：按布局记录估算 |
| 无需先校准画布即可划线 | 支持 | 不支持：先在画布上滚动或中键平移一次 |
| 组接口端口 | 支持 | 不支持 |

在 2026 上，插件只通过 `GraphNode` 的类型化属性读取节点和视图布局。每个字典、元组和数值仍会检查，不匹配时拒绝该次读取。`group_native_access.h` 中 2027 的内存偏移和模块哈希保持不变，在 2026 上不启用。

2026 上的画布校准不会因图事务（例如自动创建 Bump 节点）或画布外单击而失效。事务完成后，插件按相同的视图键、缩放和偏移找回同一个节点编辑器视图；找不到唯一匹配时，需要重新滚动画布。双击进入组总会清除校准。2027 保持原有规则。

## 构建

把 Cinema 4D 2026.0.0 C++ SDK 解压到仓库根目录的 `sdk-2026.0.0/`（不纳入 Git），然后运行：

```powershell
Expand-Archive -LiteralPath .\Cinema_4D_CPP_SDK_2026_0_0.zip -DestinationPath .\sdk-2026.0.0
.\tools\build-2026.ps1
```

脚本会检查 SDK 的 API 和 ABI 版本都是 `2026000`，并拒绝更新的 SDK（例如 2026.4 安装目录附带的 SDK）。构建目录固定为 `build-c4d-2026.0.0/`，不复用其他 SDK 的缓存。默认使用 Visual Studio 2022；如需其他工具链，传入 `-Generator` 和 `-PythonExecutable`。

2026 SDK 通过 `projectdefinition.txt` 发现插件。插件的 `CMakeLists.txt` 调用 SDK 自带的生成器转换该文件后直接返回；2027 SDK 不读取 `projectdefinition.txt`，使用该代码块之后的 CMake 设置。两个文件中的依赖列表和模块 ID 需保持一致。

## 打包

```powershell
.\tools\package.ps1 -BuildDirectory build-c4d-2026.0.0 -PackageName NodeStroke-C4D2026-preview -Readme docs/package/README-2026.txt
```

产物：`dist/NodeStroke-C4D2026-preview.zip`。

## 测试

- 模型测试：`ctest --test-dir build -C Debug --output-on-failure`
- 组端口标签测试：`py -3 -m unittest discover -s tests -p test_*.py`
- 宿主注册检查：在已加载插件的 Cinema 4D 中运行 `tests/host_smoke.py`，11 个命令必须全部存在。

已在 Cinema 4D 2026.4.0 界面中确认：插件加载、I 连接、Y 切断、撤销，以及自动创建 Bump 或 Output 节点后无需滚动即可继续划线。

发布正式版前仍需检查：

- 多个节点编辑器窗口及组之间的切换
- 摇晃摘线和拖入插线
- 100% 和 150% 显示缩放
- 用 Visual Studio 2022（v143）工具链构建

## 诊断

在“编辑 > 设置 > NodeStroke”中打开日志。日志写入偏好设置目录下的 `nodestroke_debug.txt`，记录画布绑定的变化，以及每次手势的命中、连接计划和提交结果。
