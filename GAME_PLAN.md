# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that renders GTA V graphics on screen.

**Rule:** Never stop. Update after each phase. Just work.

**Current Phase: 9 — GPU 命令翻译 → 真实游戏画面** ⚡ NEXT

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
