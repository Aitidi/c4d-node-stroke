# Cinema 4D 2026 适配记录

## 构建基线

使用用户提供的 `Cinema_4D_CPP_SDK_2026_0_0.zip`，解压到 `sdk-2026.0.0/`。
`operatingsystem.h` 的 `API_VERSION`、`apibase_version.h` 的 ABI 均为 `2026000`。
构建目录固定为 `build-c4d-2026.0.0/`，不复用本机 2026.4 随附 SDK 的缓存。
SDK 框架和 CMake 文件未修改。

```powershell
Expand-Archive -LiteralPath .\Cinema_4D_CPP_SDK_2026_0_0.zip -DestinationPath .\sdk-2026.0.0
.\tools\build-2026.ps1
```

默认使用 Visual Studio 2022。此次本机安装的是 Visual Studio 2026 / MSVC 19.50，验证命令为：

```powershell
.\tools\build-2026.ps1 -Generator 'Visual Studio 18 2026' -PythonExecutable '<path-to-python.exe>'
```

2026 SDK 先扫描 `projectdefinition.txt`。插件入口调用 SDK 自带生成器，把该定义转换到构建目录的 CMake 文件，再加载目标。2027 保留原来的 CMake 入口。

## 运行时适配范围

- `native_layout.cpp` 接受 2026 系列和原有的 2027.0.0。通过 `GraphNode` 读取类型化属性，保留字典、元组、有限数值及记录数量检查；不匹配时拒绝该次读取。
- 2026 使用已有的滚动/中键平移校准路径。节点和端口位置属于估算，不等于精确绘制数据。
- `group_native_access.h` 的 2027 模块哈希与内存偏移保持不变。2026 不启用精确绘制读取、免校准画布绑定和组接口端口操作。
- 该版本是兼容预览；已在 2026.4.0 宿主完成核心手势回归，尚未覆盖全部 GUI 与 DPI 组合。SDK 编译不能替代宿主测试。

## 打包

```powershell
.\tools\package.ps1 -BuildDirectory build-c4d-2026.0.0 -PackageName NodeStroke-C4D2026-preview-r7 -Readme docs/package/README-2026.txt
```

产物：`dist/NodeStroke-C4D2026-preview-r7.zip`。包内含插件、资源和明确标注限制的说明。

## 验收

### 用户实测与 Bump 后激活问题

用户已在 2026.4.0 GUI 确认：插件可加载、I 连接、Y 切断、Ctrl+Z 撤销。随后反馈自动创建 Bump 节点后，有时无法再次激活划线。实际日志反复出现 `gesture: key down on an unbound canvas`。

第三版预览恢复了 `PlaceHelper` 原有的辅助节点定位、延迟布局重写和呈现器同步逻辑，Bump/Displacement 节点不再停留在 C4D 默认的画布中心位置。

第二版留下的绑定诊断日志定位到根因：手势钩子会把任意画布外左键都当成编辑器导航并清空校准，而自动创建辅助节点的操作也会收到这类消息。第三版不再因画布外单击清空绑定；滚轮或中键重新校准、活动图根变化、画布双击进入组时仍会使旧绑定失效。日志实际位于 `<用户偏好目录>/prefs/nodestroke_debug.txt`。

复测重点：自动创建 Bump 后不滚动、不切换面板，立即再次按 I/Y；再等 3 秒重复；验证新增节点按原逻辑出现在源节点右侧；最后验证 Ctrl+Z 能撤回新增节点与连接，且不产生额外布局撤销步骤。

第四版预览补充了手势执行诊断：日志会记录笔画采样数、视口参数、命中节点数、起止端口是否被精确命中、连接计划数量和事务提交结果。这可区分节点命中、端口兼容性和宿主提交三类问题；诊断只在偏好设置打开日志时写入。

第五版预览修复了 Bump 自动创建后的绑定丢失。R4 日志确认连接事务成功后，C4D 2026 会替换活动图根节点的 `GraphNode` 包装对象；旧代码把这次包装对象身份变化误判为材质切换并清空绑定。现在绑定同时保存根图和容器的稳定路径，只要路径仍属于同一张图，就自动迁移到事务后的新包装对象；真正切换材质时仍会清除旧绑定。

R5 在宿主的复测仍显示 `retained 0 of 2 binding(s)`。R6 日志进一步确认，辅助节点事务甚至会替换图模型、材质包装对象和根路径，因此不能只靠这些身份迁移画布绑定。R7 改为用节点编辑器视图键以及当前缩放、水平和垂直偏移定位事务后的同一视图；画布校准描述的正是这些视图状态。若视图变换无法唯一匹配，绑定仍会失效并要求重新滚动校准。

2026-11-02 本机结果：2026.0.0 SDK Release 编译成功；6 项 C++ 模型测试、3 项 Python 标签匹配测试通过。R7 在 2026.4.0 GUI 中由用户实测：插件加载、I 连接、Y 切线、Ctrl+Z 撤销均可用；自动创建 Bump 或 Output 连接触发图事务后，无需滚动、缩放或移动画布即可继续按 I/Y，日志连续记录 `retained 1 of 1 binding(s)`。构建脚本实测会拒绝此前从 2026.4 安装目录提取的较新 SDK。

c4dpy 2026.4.0 的独立偏好目录检查在执行 Python 脚本前以 `0xC0000005` 退出；移除 NodeStroke 加载路径的对照运行同样退出，未出现基线成功标记。这不能判定插件注册或 GUI 功能通过，也不足以定位崩溃原因。日志保留于本地 `work/host-2026/`，不纳入安装包。未改动已打开的 GUI 场景，也未安装插件到用户插件目录。

构建使用本机 MSVC 19.50，而非官方示例的 VS2022 v143；仍需在受支持工具链和目标宿主中做发布前验收。该包只作为兼容预览分发。

核心模型回归：`ctest --test-dir build -C Debug --output-on-failure`。
组端口标签匹配：`py -3 -m unittest discover -s tests -p test_*.py`。
宿主注册检查：在已加载插件的 C4D 中运行 `tests/host_smoke.py`，全部 11 个命令必须存在。

后续 GUI 验收应覆盖：新建 Redshift 材质、滚动校准、I 连线、Y 切线、摇晃摘线、拖入插线、Ctrl+Z、多窗口、组切换、100%/150% DPI，以及 2027 原有功能回归。
