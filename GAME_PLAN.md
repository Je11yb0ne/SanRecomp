# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that renders GTA V graphics on screen.

**Rule:** Never stop. Update after each phase. Just work.

**Current Phase: 10 — rexglue Vulkan 渲染诊断** ⚡ NEXT

---

## 已完成阶段

### Phase 1-8: 构建 → PPC 启动 → 崩溃修复 → Vulkan → 渲染循环 ✅

| Phase | 成果 |
|-------|------|
| 1-2 | SanRecomp.exe 编译/链接/运行 |
| 3 | XEX 加载 + PPC 入口执行 (0x83639888) |
| 4 | D3D12 紫色窗口 + 最小渲染 |
| 5 | PPC 启动诊断 — 零初始化内存根因 |
| 6 | 9 项内核补丁（空页哨兵、间接调用守卫、内存分配器）|
| 7 | 启动链补全：KeBugCheck 静默、TLS 递归打破、回调分发器覆盖 |
| 8 | **Vulkan 后端 + 渲染循环激活 + VdSwap 调用** |

### Phase 8 详细成果
- ✅ Vulkan 后端：`g_backend = Backend::VULKAN`
- ✅ vulkan-1.dll 已部署
- ✅ Intel D3D12 问题彻底绕过
- ✅ VBlank 60Hz → sub_822D41E8 → VdSetDisplayMode → VdSwap → Video::Present()
- ✅ 渲染循环完整链路打通
- ❌ 画面仍是蓝色/紫色——GPU 命令缓冲未被翻译为 Vulkan 绘制调用

---

## Phase 9: GPU 命令翻译 → 真实游戏画面

**问题：** 游戏调用了 VdSwap，画面在刷新，但显示的是清除色而非游戏内容。
**根因：** Xbox 360 GPU 命令（PM4 包/ring buffer）未被翻译为 Vulkan 绘制调用。
**目标：** 让游戏中的 3D 几何体、纹理、着色器真正渲染到窗口。

### 当前链路 vs 目标链路

```
当前：
VBlank → sub_822D41E8 → VdSwap → _VideoPresent() → plume Present → 蓝色/紫色

目标：
VBlank → sub_822D41E8 → GPU 命令 → Vulkan 绘制 → 纹理/几何体 → plume Present → 游戏画面
```

### 核心任务

1. **启用视频管线代码**
   - 解开 `gpu/video.cpp` 中 `#if 0` 的管线设置代码
   - 适配 Vulkan 后端（原代码为 D3D12 编写）

2. **GPU 命令缓冲翻译**
   - 实现 PM4 命令包的 Vulkan 翻译
   - 参考 `refs/UnleashedRecomp/UnleashedRecomp/gpu/video.cpp`（7500+ 行）
   - 参考 `refs/rexglue-sdk/src/graphics/vulkan/`（完整实现）

3. **着色器系统**
   - 运行 XenosRecomp 将 Xbox 360 .fxc → SPIR-V
   - 或复用 UnleashedRecomp 的着色器缓存

4. **纹理/资源加载**
   - 确保 RPF 资源提取正确
   - 确保纹理能传递到 Vulkan

---

## Phase Progress Log

### 2026-06-18 (Phase 10 — rexglue Vulkan 渲染诊断)

**🟢 重大突破：rexglue Vulkan 窗口 + Presenter 工作！**

- ✅ **Vulkan Presenter 创建成功** (presenter=YES)
  - 关键：`runtime.set_app_context(&app_context)` 必须在 `runtime.Setup()` 之前调用
  - 这样 Runtime 内部的 `SetupPresentation` 才会被调用
- ✅ **Win32 窗口弹出** — `Window::Create + Open + SetPresenter`
- ✅ **Setup OK, XEX loaded, Launched** — 游戏完整启动链打通
- ✅ **VEH 页面错误处理** — `AddVectoredExceptionHandler` 自动提交内存页面
- ⚠️ **窗口黑屏** — 游戏运行但画面全黑
- 🚫 **教训：禁止 VirtualAlloc GPU MMIO (0x7FC80000)** — 会与 GPU 驱动冲突导致蓝屏

**🔴 黑屏根因已确诊（2026-06-18 晚）：FATAL Unresolved call，不是忙等也不是 GPU 翻译问题**

诊断方法（systematic-debugging）：
- 用 `REX_HOOK_RAW` 覆盖弱别名函数加追踪（hook 机制经 xstart 验证有效）
- 启用 rexglue 自带日志（`rex::InitLogging(debug, flush=trace)` → `gta5_kernel.log`）

诊断证据：
- `xstart ENTERED` 但**无 `xstart RETURNED`** → 游戏入口执行了但卡在 xstart 内部
- 渲染函数 `sub_822D41E8` **从未被调用** → 从未到达渲染循环（排除"GPU 命令未翻译"假设）
- 内核日志末行：`[critical] [FATAL] Unresolved call from 0x8255DC5C to 0x8255DC48`

根因：
- `sub_8255DC58` = thunk：`li r4,4; b 0x8255DC48`（尾调用跳到 0x8255DC48）
- 0x8255DC48 在 `sub_8255DC20`（C++ vtable 分发器）的 `bctr` **之后**，重编译器把 bctr 当终止符，**没生成 0x8255DC48 的代码** → 函数边界识别遗漏
- guest 主线程撞上 `REX_FATAL` → 线程死 → init 中断 → 黑屏（窗口存活因主线程在消息循环 + GPU/VSync 线程在跑）

问题规模：**仅 8 个 FATAL，4 个唯一未解析目标**：`0x8255DC48` `0x8255DC88` `0x8256BCC8` `0x83366BD0`

修复路径（已确认）：在 `gta5_manifest.toml` 加 `[functions]` 声明这 4 个地址为函数入口 → 重新 `rexglue codegen` → 重新构建：
```toml
[functions]
0x8255DC48 = {}
0x8255DC88 = {}
0x8256BCC8 = {}
0x83366BD0 = {}
```

**构建系统教训：`.ninja_deps` 被中途杀构建/截断管道损坏 → 每次全量重编 197 文件。修复：删 `.ninja_deps`+`.ninja_log` 后做一次完整不中断构建；之后增量构建正常。绝不中途杀 ninja。**

**🟢 修复已实施 + 游戏推进到完整 init（2026-06-18 深夜）**

- ✅ schema 修正：嵌套 manifest 必须用 `[entrypoint.functions]`（不是顶层 `[functions]`），`end` 用引号字符串
- ✅ 加了 5 个缺失函数（静态 4 + 运行时发现的 0x836E9A58、0x836E9A70）→ 0 个 FATAL
- ✅ **main.cpp 覆盖 `rex::runtime::ResolveIndirectFunction`** 作批量收集器：缺失函数不再 FATAL，而是记录到 `gta5_missing_funcs.txt` + 返回空 stub（链接通过，无重复符号）
- ✅ **游戏现在跑完整早期 init**：Bink 视频线程(BinkAsy1)、XamNotifyCreateListener、XamContentCreateEnumerator、**SetInterruptCallback(836B1768) 注册 GPU 中断回调**（渲染路径！）
- ⚠️ **新阻塞：游戏在 SetInterruptCallback 后挂起**（39s 无内核活动）— 疑似等 VBlank/GPU 中断触发回调（同 Phase 8 模式），或等 gameconfig.xml 加载
- ⚠️ `game:\common\data\gameconfig.xml` VFS 未找到（GTA V 数据在 .rpf 归档）

**IDA 调查结论（不可行）**：那个 106MB `.i64` 只有 1 段（0x0，原始压缩 XEX 当扁平数据）+ 1 函数——从没用 XEX loader 正确加载/分析。两个 IDA 安装都没 Xbox360 XEX loader。MCP 的 idalib（C:\software\IDAPRO 补丁版）也读不了。要用 IDA 需编译 idaxex 匹配 9.3 SDK + 正确加载 + 数小时分析，不值得。**缺失函数只能运行时逐个发现（稀疏，可控）。**

**下一步**：诊断 SetInterruptCallback 后的挂起 — rexglue GPU vsync worker 是否 DispatchInterruptCallback 触发游戏回调 836B1768？游戏在等什么 GPU 状态？

**🟢 VBlank 派发已确认 + 全线程死锁定位（2026-06-18 深夜2）**

- ✅ **rexglue 确实 60Hz 派发 VBlank** 给游戏中断回调 sub_836B1768（hook 计数：30s 内 1620+ 次 ≈54Hz，r4=0x4004CD00 正确 user_data）。vsync worker → MarkVblank → DispatchInterruptCallback → ExecuteInterrupt 链路工作。
- ⚠️ **但 init 仍不前进——根因是多线程同步死锁，不是缺 VBlank**
- 诊断方法：看门狗线程采样主线程 PPCContext（lr/r1/r3），+ kernel_state object_table dump 所有线程
- **主线程（id=4）阻塞在 `NtWaitForSingleObjectEx`**（sub_823C7A68 wait 包装，lr=0x823C7AA0），等**信号量句柄 0xF80008F0**（object type=8=Semaphore），22s 完全静止
- **全部 18 线程都在等**：
  - id=4~F（12线程）：NtWaitForSingleObjectEx 等各自信号量
  - id=11/12：KeWaitForSingleObject 等调度对象指针（在 GPU 代码 0x836BExxx）
  - id=10：**RtlEnterCriticalSection** 等被占的临界区锁（0x820108EC）
  - id=1/2/3：host 线程（GPU Commands/VSync/Kernel Dispatch）
- GPU 中断处理器 836B1768 只做自旋锁，不直接释放这些信号量
- **疑似根因**：(a) 游戏 init 工作线程池死锁——没有线程释放信号量启动工作；或 (b) gameconfig.xml 缺失导致加载线程错误/挂起不发完成信号；或 (c) rexglue 信号量/临界区唤醒有问题

**下一阶段**：解开这个多线程死锁 — 追踪信号量释放路径（谁该 release 0xF80008F0），对比可用 rexglue 项目（TDURE）的线程行为，或检查 gameconfig.xml 加载路径。诊断工具已在 main.cpp（看门狗 + objdump）。

**🔴 死锁深挖（2026-06-18 深夜3）：游戏 29 次 WAIT，0 次 RELEASE**

- ✅ gameconfig.xml 已提供（从 `gta5 extract/common.rpf/data/` 合并进 `gta5/common/data/`，VFS 不再 miss）——**但死锁依旧**，证明 gameconfig 不是根因（红鲱鱼；不过提取的资产过了死锁后仍需要）
- ✅ rexglue `NtWaitForSingleObjectEx` 实现正确（LookupObject→object->Wait，非 stub），wait 机制无 bug
- ✅ 排除全局锁死锁（线程分散在不同 wait 函数，全局锁在等待时正常释放）
- ✅ **hook 游戏 wait 包装 sub_823C7A68 + 释放包装 sub_8239CBA8 追踪信号量操作**
- 🔴 **关键发现：16s 内 29 次 WAIT，0 次 RELEASE** — 游戏从不释放任何信号量
  - 主线程 tid=4 等 0xF80008F0（一次，永久阻塞）
  - tid=6/7 **循环重试**等 0xF8000068（带超时，超时→重等，有执行时间但条件永不满足）
- **根因结论**：生产者/释放侧从未运行 → 所有消费者线程阻塞。最可能是 GPU/渲染线程（tid=11/12，阻塞在 KeWaitForSingleObject 等调度对象 0x4004FB4C/0x4004FBB8，在 GPU 代码 0x836BExxx）是该释放信号量的生产者，但它在等某个永不就绪的 GPU 状态/事件；GPU 中断处理器 836B1768 只做自旋锁，不 signal 它

**下一阶段策略选项**：(a) 追 tid=11/12 在 GPU 代码里等的调度对象，连接到 GPU 中断该 signal 的路径；(b) 对比 UnleashedRecomp（refs/，完整工作的全游戏 recomp）的线程/GPU 同步实现；(c) 追 tid=6/7 循环里检查的条件

### 2026-06-17 (Phase 9 — GPU 命令翻译 WIP)
- ✅ SPIR-V 着色器编译：DXC 将 22 个 HLSL → SPIR-V .h 文件
- ✅ video.cpp SPIR-V 包含解除 + CREATE_SHADER 宏修复 Windows+Vulkan
- ✅ `#if 0` 管线屏障移除，SEH 回退到最小渲染模式
- ✅ VBlank 60Hz 回调确认工作，sub_822D41E8 被调用
- ✅ VdSwap → _VideoPresent() → Video::Present() 链路完整
- ✅ plume swap chain Present 调通（但无可呈现帧 — "Swapchain starved"）
- ⚠️ 全管线在空纹理创建处崩溃（0xC0000005），SEH 回退到最小模式
- ⚠️ PPC 主线程卡在忙等：轮询地址 0x838871A4，值从 0x10→0xFFFFFFFF 保持循环
- ⚠️ PPC_LR=0x822F4CC8（BSS 代码区），忙等检测器正常触发但无法打破
- ⚠️ VBlank 回调中 interruptUserData=0，导致 sub_822D41E8 的 r31=0，函数无法正常工作
- ⚠️ 游戏未完成初始化，无法注册真实的 VdSetGraphicsInterruptCallback
- ✅ 添加 init loop breakers + sub_83639628 覆盖（返回 0 跳过 XamLoaderTerminateTitle）
- ✅ 忙等破解器值轮转：0x10→0x00→0xFF→… 推动游戏状态机前进
- ✅ **游戏初始化完成！_xstart 成功返回**（用 0x00 破解忙等后）
- ✅ SDL 事件循环保持窗口存活（main.cpp）
- ✅ VBlank 从事件循环启动（而非 guest_thread），60Hz 持续运行
- ✅ sub_822D41E8 覆盖为 no-op 防止 VBlank 线程挂起
- ✅ VBlank PCR 在 0x82001000 初始化（有效 PCR 结构）
- ✅ VdGetSystemCommandBuffer 返回有效 ring buffer (0x84000000)
- ✅ **连续帧呈现：60Hz PrepareFrameAndPresent 循环正常工作**
- ✅ PrepareFrameAndPresent()：acquire→clear→barriers→execute→present 完整周期
- ✅ 帧 #1-#5, #60, #120, #180, #240, #300+ 全部成功呈现
- ✅ VBlank 线程不间断运行，无阻塞无崩溃
- ✅ **游戏渲染回调 sub_822D41E8 成功执行并调用 VdSwap**（重大突破！）
- ✅ 原始 PPC 渲染函数运行并通过 `__imp__sub_822D41E8` 调用 VdSwap
- ✅ **稳定 60Hz VBlank 循环** — 无崩溃，连续帧呈现
- ✅ **直接渲染回调注册** — 通过 g_gpuRingBuffer 绕过损坏的初始化链
- ✅ **DEVELOPMENT_HACKS.md 已创建** — 所有变通方法的全面目录
- ✅ **rexglue-sdk 内核分析** — 缺失的服务不会被游戏调用
- ✅ **图形状态实验** — 测试了三种方法（零值、自引用、小值）
- ⚠️ 渲染回调在 VdSwap 后崩溃 — 图形状态需要特定值（指针=0，计数≠0）
- ⚠️ 环形缓冲区仍为空 — 崩溃前未写入 PM4 命令
- 🔧 下一步：对 0x83830000 处的图形状态结构进行 IDA 逆向工程

### 2026-06-17 (Session — rexglue 混合架构实验 + 回退)

- ⚠️ rexglue 混合架构（rexglue PPC 文件 + XenonRecomp 运行时）导致系统死机
- ✅ **根因分析**：rexglue PPC 文件（`rex::ppc::PPCContext`）与旧运行时不兼容
- ✅ `dumpbin` 确认 WinRT DLL 依赖来自 `media_win32.cpp`（非 rexruntime）
- ✅ `media_win32.cpp` WinRT GSMTC → stub（去 `api-ms-win-core-winrt-error` 依赖）
- ✅ `dxcompiler.dll` + `dxil.dll` 部署到 exe 目录
- ✅ 回退到 XenonRecomp 纯版本（分支 `feature/xenonrecomp-phase9`）
- ✅ exe 启动验证：无 DLL 缺失、无系统死机、XEX 加载正常
- ✅ 创建 `REXRUNTIME_FIX_AND_PROJECT_PLAN.md` — 7 阶段完整规划
- 🔧 **策略决定**：rexglue 作为参考源码（内核/Vulkan/PM4），不改运行时
- ✅ **Phase 3 完成**：内核同步/计时器服务补全
  - KePulseEvent、KeInitializeEvent（新实现）
  - Timer 内核对象类 + NtCreateTimer/NtSetTimerEx/NtCancelTimer
  - KeInitializeTimerEx/KeSetTimerEx/KeCancelTimer
  - KeWaitForMultipleObjects 修复（从 always-success → 真正的 WaitAll/WaitAny 轮询）
- ✅ **Phase 5 调查完成**：GPU 诊断钩子 + PM4 环形缓冲区扫描
  - 添加 D3D 诊断钩子（CreateTexture、DrawIndexedPrimitive 等 8 个函数）
  - 添加 VdSwap 中 PM4 环形缓冲区扫描
  - **关键发现**：GTA V 不调用 D3D wrapper 函数，也不写 PM4 命令缓冲
  - VdSwap 每帧调用（游戏认为自己渲染了），但无实际绘制
  - TDURE 参考项目调查（rexglue 0.7.4 完整集成，仅 D3D12）
  - Xbox-360-Crypto 参考调查（Python crypto，对我们无用）
- 🔧 **下一步**：调查游戏 GPU 初始化缺失了什么 → 让游戏开始画东西

### 2026-06-17 (Session 2 — rexglue 正确集成)

- ✅ 读取 rexglue-sdk wiki（Getting-Started、ReXApp、Function-Overrides）
- ✅ test_rexglue 构建成功（70/70, 197 rexglue PPC 文件 + 预编译 SDK）
- ✅ 改用 `find_package(rexglue)` + `rex::runtime` 正确链接
- ✅ GTA5App 简化（TDURE 模式）+ REX_DEFINE_APP 入口
- 🔧 构建+部署 DLL → 测试启动中

### 2026-06-16 (Session — Vulkan 迁移 + 渲染循环)
- ✅ 研究 refs/ 所有项目渲染后端
- ✅ Vulkan 后端启用，D3D12 代码路径替换
- ✅ VBlank 60Hz 渲染驱动
- ✅ 找到 GTA V 渲染函数 sub_822D41E8
- ✅ VdSetDisplayMode + VdSwap 被游戏调用
- ✅ 渲染循环完整：VBlank → VdSwap → Video::Present
- ✅ Switch 移植路径确认（Plume Vulkan → NVK）
- ✅ rexglue wiki 学习 + 记录到 CLAUDE.md
- ✅ 共 38 commits 推送到 main

### Past sessions
- Phase 1-7: see git log for details
