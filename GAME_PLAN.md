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

## Phase 5: rexglue 迁移 — PPC 启动调试 ✅ (诊断完成)

**决策 (2026-06-16):** rexglue-sdk v0.8.0 作为主要重编译器，XenonRecomp 保留辅助
**理由:** 310 import symbols vs 50, built-in Runtime, 5.6M instruction analysis
**诊断结论:** XenonRecomp 的零初始化 PPC 内存导致游戏的未初始化数据结构引起各种循环/崩溃

## Phase 6: rexglue 集成 (NEXT ⭐)

**任务:**
1. 用 rexglue 生成的代码替换 XenonRecomp PPC 代码
2. 集成 rexglue Runtime（kernel, D3D12, audio, input）
3. 对比 rexglue 的 310 导入实现 vs 我们当前的 ~100 imports
4. 验证游戏可以完成启动初始化

**rexglue 生成的文件:** `refs/rexglue-sdk/rexglue-bin/win-amd64/test_gta5/generated/default/`
- 200+ recomp 文件 (gta5_test_recomp.*.cpp)
- gta5_test_init.cpp/h: 函数注册表 (~83K 条目)
- gta5_test_register.cpp: 函数映射表

## Phase 5 Progress Log

### 2026-06-16 (session 2 — PPC boot debugging)
- ✅ CI workflow 文件修复：LibertyRecomp → SanRecomp（防止 GitHub Actions 邮件轰炸）
- ✅ ExGetXConfigSetting(2,2) 返回值修复：0x1000 → 0x300 (GTA V 校验兼容)
- ✅ 空页哨兵初始化：地址 0 的链表遍历不再无限循环 (memory.cpp)
- ✅ 函数表保护移除：GTA V 需要写入 0x83DC0000+ 范围的运行时数据
- ✅ KeBugCheckEx/KeBugCheck 改为日志记录而非崩溃
- ✅ RtlEnterCriticalSection/RtlLeaveCriticalSection null 指针保护
- 🔄 当前阻塞：sub_8363E1D8 链表遍历忙等 (LR=0x8363E1E0, r1=0xFFFFFE80)
- 📊 游戏已可运行多个线程、调用多个内核函数、在遇到严重错误时主动调用 KeBugCheckEx(0xF4)
- 💡 根本问题：XenonRecomp 重编译的代码依赖已初始化的数据结构（BSS/data sections），但 PPC 内存是从零开始的

### 2026-06-16 (previous session)
- ✅ 最小 D3D12 clear+present — 窗口显示紫色
- ✅ BeginCommandList null-guard 修复 — 跳过 descriptor heap 绑定
- ✅ 直接渲染到 swap chain backbuffer — 跳过中介纹理 + gamma correction

## Phase Progress Log (continued)
- ✅ PPC 看门狗 + 内核调用追踪系统
- ✅ 6 个参考项目下载到 refs/
- ✅ rexglue-sdk v0.8.0 分析 GTA V XEX 成功
- ⏳ PPC 忙等根因待定位

### 2026-06-15 (initial)
