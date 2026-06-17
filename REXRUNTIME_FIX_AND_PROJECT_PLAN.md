# SanRecomp 项目总览与 rexruntime DLL 阻塞修复规划

日期：2026-06-17

## 1. 当前项目定位

SanRecomp 是将 GTA V Xbox 360 版本静态重编译到 PC 的工程。当前工程由三类体系混合组成：

1. `SanRecompLib/ppc/`：XenonRecomp 生成的 PPC 到 C++ 重编译代码（546 个文件）。
2. `SanRecomp/`：宿主运行时，包括 kernel stub、GPU（D3D12/Vulkan via plume）、输入、音频、UI、installer 等。
3. `refs/rexglue-sdk/` + `tools/rexglue-sdk-*/`：当前选定为主参考/主重编译方向的 rexglue-sdk v0.8.1.32-dev，带更完整的 kernel/runtime/graphics/audio/input 支撑。

当前阶段原目标是 Phase 9：GPU 命令翻译，从蓝屏 clear/present 走到 PM4 命令到 Vulkan/D3D 绘制。但现阶段存在更靠前的硬阻塞：`SanRecomp.exe` 运行时加载预编译 rexruntime DLL 失败，导致无法进入后续 kernel/GPU 调试。

## 2. 当前最高优先级阻塞

### 现象

- exe 无法运行。
- Windows loader 报缺失 `api-ms-win-core-winrt-error-l1-1-1.dll`。
- 注释中怀疑是预编译 `rexruntime.dll` (release 版) 有问题，当前改用 `rexruntimerd.dll`，但实际 exe 导入表中直接引用了 `api-ms-win-core-winrt-error-l1-1-1.dll`。
- `dumpbin /imports` 确认 exe 直接从 `api-ms-win-core-winrt-error-l1-1-1.dll` 导入 `RoOriginateLanguageException`。
- 该符号来自 `media_win32.cpp.obj` 中的 C++/WinRT GSMTC 代码，而三个预编译 rexruntime DLL 都没有引用该 API Set。
- **结论：缺失 DLL 不是 rexruntime 的问题，而是项目自己编译的 `media_win32.cpp` 用 C++/WinRT 导致链接器引入的依赖。**

### 修复（已应用）

- `SanRecomp/os/win32/media_win32.cpp`：把 GSMTC 查询替换为直接返回 `false` 的 stub。
- 重新链接后 exe 导入表不再包含 `api-ms-win-core-winrt-error-l1-1-1.dll`。
- 用 `Start-Process` 测试，exe 退出码 `-1073741515`（= `0xC0000135` = `STATUS_DLL_NOT_FOUND`），缺 `dxcompiler.dll` 和 `dxil.dll`，不再是 WinRT error。

### 下一步行动

1. 部署 `dxcompiler.dll` + `dxil.dll` 到 exe 目录。
2. 验证 exe 能正常启动进入窗口/日志。
3. 确认所有运行时 DLL 满足后，进入 Phase 1 诊断链。

## 3. 已验证的技术事实

| 事实 | 来源 | 结论 |
|------|------|------|
| `rexruntimerd.dll` 不引用 `api-ms-win-core-winrt-error-l1-1-1` | `dumpbin /dependents` + `/imports` | 不是 rexruntime 的问题 |
| `rexruntime.dll` (release) 也不引用 | 同上 | 同上 |
| `rexruntimed.dll` (debug) 也不引用 | 同上 | 同上 |
| `SanRecomp.exe` 导入表中有 `api-ms-win-core-winrt-error-l1-1-1.dll` | `dumpbin /dependents` | 项目自身代码引入 |
| `media_win32.cpp.obj` 有 `WINRT_IMPL_RoOriginateLanguageException` | `dumpbin /directives` + `/symbols` | C++/WinRT GSMTC 代码是根因 |
| 修复后 exe 退出码 `-1073741515` (DLL_NOT_FOUND) | PowerShell `Start-Process` | 缺 `dxcompiler.dll` |
| `dxcompiler.dll` 不在 exe 目录 | `Get-ChildItem` 检查 | 需要复制 |
| `test_rexglue/` 已单独编译验证 rexglue codegen | `main.cpp` 编译链接通过 | rexglue 生成代码可用 |

## 4. 项目当前主要不足

1. **Runtime 路线混杂**：当前存在 SanRecomp 自有 runtime、LibertyRecomp/GTA IV 遗留、UnleashedRecomp 参考、rexglue runtime 四套假设，边界不清。
2. **构建系统脆弱**：手动 `target_link_libraries` 指定无数预编译 lib，Debug/Release/RelWithDebInfo 的选择缺乏自动化约束。
3. **Kernel imports 覆盖不足**：rexglue 分析显示 GTA V 有约 310 个导入，当前项目只覆盖约 50 个 stub。
4. **同步/计时器服务不足**：事件、timer、wait 等函数不完整，容易导致 PPC 忙等。
5. **GPU 命令翻译未闭环**：当前能 clear/present，但还没有稳定 PM4 到 draw call 的路径。
6. **GTA IV/Sonic 遗留较多**：patches、API 命名、寄存器地址、结构体偏移仍有旧项目假设。
7. **诊断体系不够固定**：需要稳定日志、导入覆盖表、PM4 trace、kernel trace，而不是临时 printf。

## 5. 分阶段规划

### Phase 0：解除 exe loader 阻塞 ✅（已完成根因诊断 + 代码修复，待验证）

**目标**：`SanRecomp.exe` 能启动，不再因缺失 DLL 失败。

**已完成**：
- ✅ `dumpbin` 确认三个预编译 rexruntime DLL 都不引用缺失 API Set。
- ✅ 定位根因：`media_win32.cpp` 的 C++/WinRT GSMTC 代码。
- ✅ 代码修复：用直接返回 `false` 的 stub 替换 WinRT 实现。
- ✅ 重新链接，验证 exe 不再导入 `api-ms-win-core-winrt-error-l1-1-1`。

**待完成**：
- 部署 `dxcompiler.dll` 到 exe 目录。
- 验证 exe 能正常启动。
- 更新 CMakeLists.txt 注释，移除关于 rexruntime 版本选择的误导性注释。

### Phase 1：建立可重复诊断链

**目标**：每次运行都有可比较证据。

**任务**：
- 启动日志记录：exe 路径、runtime DLL 路径、构建类型、SDK/clang 版本。
- PPC 看门狗输出 PC/r1/r3/调用计数。
- kernel import trace 改为按函数计数，避免刷屏。
- GPU trace 记录 ring buffer、PM4 packet、未知 opcode。

**验收**：
- 每次运行生成稳定日志文件。
- 能比较两次运行的推进点差异。

### Phase 2：清理构建系统

**目标**：构建不再依赖偶然路径和手工 DLL 混搭。

**任务**：
- 增加 `REXGLUE_ROOT` / `REXGLUE_RUNTIME_DIR` 变量。
- 按 CMake config 选择对应 runtime。
- 检查 DLL/lib 是否成对存在。
- 配置阶段输出 runtime 摘要。
- 统一库命名（去 rd/d 后缀混乱）。

**验收**：
- 干净构建目录可复现 configure/build。
- 错误尽早在 configure/build 阶段暴露。

### Phase 3：补齐 kernel 同步与计时器服务

**目标**：让游戏初始化继续推进，避免卡在忙等。

**优先函数**：
- `KeInitializeEvent`, `KeSetEvent`, `KeResetEvent`, `KePulseEvent`
- `KeWaitForSingleObject`, `KeWaitForMultipleObjects`
- `KeInitializeTimerEx`, `KeSetTimerEx`, `KeCancelTimer`
- `NtCreateEvent`, `NtSetEvent`, `NtWaitForSingleObject`

**参考**：
- `refs/rexglue-sdk/src/kernel/`
- Xenia kernel object/event/timer 模型
- UnleashedRecomp kernel patch

**验收**：
- PPC 看门狗显示 PC/r1 持续推进。
- kernel trace 不再重复卡在同一个 wait/timer 函数。
- 游戏初始化能走到图形资源创建或命令提交。

### Phase 4：导入表系统化补全

**目标**：从"遇到一个补一个"变成按导入表管理。

**任务**：
- 从 rexglue 生成完整 310 imports 清单。
- 标注已实现、可安全 stub、危险 stub、必须实现。
- 按 xboxkrnl/xam/xnet/d3d/audio 分类。
- 对危险 stub 加一次性日志和调用计数。

**验收**：
- 形成 `docs/import_coverage.md`。
- 每个未实现导入都有优先级。

### Phase 5：GPU 初始化与状态结构逆向

**目标**：修复图形状态未初始化导致的渲染回调崩溃。

**任务**：
- 分析 `0x83830000` 附近图形状态结构。
- 建立 `sub_822D41E8` 读取偏移的字段草图。
- 分批恢复 `video.cpp` 里被 `#if 0` 的 guest function hooks。
- 记录 D3D 初始化路径是否被调用。

**验收**：
- 不再只是固定蓝屏。
- PM4 ring buffer 开始出现非空命令。

### Phase 6：PM4 到 Vulkan/D3D 绘制最小闭环

**目标**：真实游戏画面第一帧。

**最小闭环**：
- PM4 packet 解析
- render target 设置
- vertex/index buffer 绑定
- shader lookup/转换
- texture/sampler 绑定
- draw indexed
- resolve/present

**参考**：
- UnleashedRecomp `video.cpp`
- XenosRecomp shader pipeline
- plume D3D12/Vulkan 抽象
- rexglue `graphics/pipeline/` 完整 PM4 命令处理器

**验收**：
- 捕获至少一个有效 draw call。
- 屏幕内容不再只是固定 clear 色。
- 日志或 RenderDoc 能对应到 guest draw。

### Phase 7：rexglue 深度集成决策

**目标**：决定长期是迁移到 rexglue runtime，还是只把 rexglue 当分析与参考工具。

**评估维度**：
- 本机 rexruntime 是否可稳定构建。
- rexglue generated code 是否比当前 XenonRecomp 输出更易接入。
- KernelState/Memory/FunctionDispatcher 接入成本。
- 当前 546 个 PPC generated cpp 是否继续保留。

**建议**：
- 短期不要一次性大迁移。
- 先解决 loader、kernel 同步、GPU 初始化。
- 等运行链路稳定后，再决定是否迁移 generated code。

## 6. rexruntime DLL 依赖问题——最终分析

### 修复总结

| 步骤 | 工具 | 发现 | 操作 |
|------|------|------|------|
| 1 | `dumpbin /dependents` (三个 DLL) | 都不导入 `api-ms-win-core-winrt-error-l1-1-1` | 排除 rexruntime 嫌疑 |
| 2 | `dumpbin /dependents SanRecomp.exe` | exe 导入 `api-ms-win-core-winrt-error-l1-1-1.dll` | 定位到项目自身 |
| 3 | `dumpbin /imports SanRecomp.exe` | 导入 `RoOriginateLanguageException` | 定位到 C++/WinRT |
| 4 | `dumpbin /directives media_win32.cpp.obj` | 60+ 条 `WINRT_IMPL_*` directive + `RoOriginateLanguageException` | 确认为 `media_win32.cpp` |
| 5 | `grep` 搜索 `winrt/` include | `media_win32.cpp` 包含 C++/WinRT GSMTC 查询 | 根因确认 |
| 6 | Edit `media_win32.cpp` | 替换为直接返回 `false` | 修复代码 |
| 7 | 重新编译链接 | 成功，`dumpbin /dependents` 不再包含 winrt-error | 验证修复 |
| 8 | `Start-Process` 测试运行 | 退出码 `-1073741515` = `STATUS_DLL_NOT_FOUND` | 缺 `dxcompiler.dll` |

### 根因

不是 rexruntime DLL 的问题。是 `SanRecomp/os/win32/media_win32.cpp` 中用 C++/WinRT 查询 Windows 系统媒体播放状态 (`GlobalSystemMediaTransportControlsSessionManager`)，clang-cl 的 C++/WinRT 实现会自动链接 `windowsapp.lib`，从而引入对 `api-ms-win-core-winrt-error-l1-1-1.dll` 的依赖。

### 当前状态

- ✅ 根因定位 + 代码修复完成。
- ⚠️ 下次需要部署 `dxcompiler.dll` 后才能验证 exe 是否完全启动。
- ⚠️ 需要把 `SanRecomp/os/linux/media_linux.cpp` 和 `SanRecomp/os/macos/media_macos.cpp` 也检查一致性。

## 7. 下一次启动点

1. 部署 `dxcompiler.dll` + `dxil.dll` 到 exe 目录。
2. 运行 exe 验证启动（检查是否还有其他缺失 DLL）。
3. 更新 CMakeLists.txt 注释，移除关于"release build has api-ms-win-core-winrt-error dependency"的误导性注释。
4. 更新 `CLAUDE.md` 和 `GAME_PLAN.md`。
5. 进入 Phase 1：建立诊断链。
