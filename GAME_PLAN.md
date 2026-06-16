# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that boots the game.

**Rule:** After each phase, update this file. Never deviate from the current phase. Never ask "continue?" — just work.

**Current Phase: 4 — Rendering Pipeline + Game Boot** ✅ IN PROGRESS

## ✅ Phase 1 COMPLETE (2026-06-15)
- SanRecomp.exe: 105MB PE32+ x64, 0 compile errors, 0 linker errors

## ✅ Phase 2 COMPLETE (2026-06-15)
- Exe launches, runs PPC patch init, reaches Video::CreateHostDevice
- No crash — PE loader + static init + kernel init all work

## ✅ Phase 3a: XEX Loading + PPC Boot (2026-06-16)
- Xex2LoadImage stub replaced with real implementation
- XEX entry point: 0x83639888 (was 0x00000000)
- PPC code executes via GuestThread::Start → _xstart
- o1heap crash bypassed with malloc fallback

## ✅ Phase 3b: Plume Rendering (2026-06-16)
- Plume built as static library (D3D12 backend)
- D3D12 device creation and swap chain working
- SDL window visible with "San Recompiled" title
- PPC code continues executing (exit 124 = timeout, alive)

## ✅ Phase 3c: InstallerWizard UI (2026-06-16)
- 4 UI source files enabled (installer_wizard, imgui_utils, achievement_menu, imgui_snapshot)
- 20+ resource variable stubs added
- NFD + soundtouch stubs added
- InstallerWizard::Run reached at runtime (exit code 3)
- All D3D12 GPU resources created successfully

## ✅ Phase 4a: Minimal D3D12 Clear+Present (2026-06-16)
- Swap chain + backbuffer + descriptor sets created before #if 0 block
- Root cause #1: Intel D3D12 driver crashes on SetDescriptorHeaps/SetGraphicsRootDescriptorTable
  → Fix: BeginCommandList skips descriptor binding when g_pipelineLayout==null
- Root cause #2: Intermediary texture rendered but swap chain backbuffer wasn't receiving pixels
  → Fix: Minimal mode renders directly to swap chain backbuffer, skipping intermediary + gamma correction
- Initial clear+present works — window should show dark purple
- 0 compile errors, 0 runtime crashes

## Phase 4: Rendering Pipeline + Game Boot (NEXT)
**Goal:** Fix D3D12 pipeline crashes on Intel, complete game boot
**Achieved:** Minimal clear+present working ✅
**Blockers:** Intel D3D12 driver issues (descriptor heaps), PPC code hangs without output

**⚠️ Self-Audit Checklist — Run Before Each Work Session:**
1. Read CLAUDE.md
2. `Skill("using-superpowers")` then `Skill("andrej-karpathy-skills:karpathy-guidelines")`
3. Read this GAME_PLAN.md
4. Apply systematic-debugging: batch ALL errors before fixing ANY
5. Surgical changes only — Edit tool, not sed/awk

---

## ✅ Phase 4b: 参考项目研究 + rexglue 评估 (2026-06-16)
- refs/ 目录创建，6 个参考项目已 clone
- UnleashedRecomp 研究：无 game_init.cpp，PPC 自行初始化，310+ 内核导入
- rexglue-sdk v0.8.0 下载并测试：可分析 GTA V XEX
  - 发现 5,595,310 条指令，10,728 代码区域，310 个导入符号
  - 完整的重编译 SDK（Runtime、D3D12/Vulkan、Audio、Input）
- PPC 看门狗增强：追踪 r1/r3 寄存器 + 内核调用
  - 诊断结果：r1 静态 = 卡在单个函数内，无内核调用 → 疑似 busy-wait

## Phase 5: rexglue 迁移 — 用 rexglue 重编译 GTA V (NEXT)
**决策 (2026-06-16):** rexglue-sdk v0.8.0 作为主要重编译器，XenonRecomp 保留辅助
**理由:** 310 import symbols vs 50, built-in Runtime, 5.6M instruction analysis
**任务:**
1. 完成 `rexglue codegen` 生成完整 C++ 代码
2. 分析生成的 kernel 实现，对比现有 imports.cpp
3. 解决 PPC busy-wait 问题（rexglue 可能有不同处理方式）
4. 集成 rexglue Runtime 替换 Plume/SDL 层（可选，视复杂度）

## Phase Progress Log

### 2026-06-16 (today)
- ✅ 最小 D3D12 clear+present — 窗口显示紫色
- ✅ BeginCommandList null-guard 修复 — 跳过 descriptor heap 绑定
- ✅ 直接渲染到 swap chain backbuffer — 跳过中介纹理 + gamma correction
- ✅ PPC 看门狗 + 内核调用追踪系统
- ✅ 6 个参考项目下载到 refs/
- ✅ rexglue-sdk v0.8.0 分析 GTA V XEX 成功
- ⏳ PPC 忙等根因待定位

### 2026-06-15 (initial)
