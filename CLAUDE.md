# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⚠️ READ THIS FIRST — Every Session

Before doing ANY work, invoke these skills in order:

1. **`Skill("using-superpowers")`** — loads the toolset
2. **`Skill("andrej-karpathy-skills:karpathy-guidelines")`** — behavioral rules, DO NOT SKIP
3. **Read this entire file** — critical project context
4. **Read GAME_PLAN.md** — current phase + progress log

Then at every step:
- **Think before coding.** State assumptions. Surface tradeoffs. No guessing.
- **Systematic debugging** — batch ALL errors before fixing one.
- **Surgical changes** — no blind `sed`/`awk`. Use Edit tool with exact text.
- **Simplicity first** — minimum code. No speculative changes.
- **End every session** by updating this file + GAME_PLAN.md with progress.

---

## Project Overview

SanRecomp is a static recompilation project porting GTA V (Xbox 360) to PC. Forked from LibertyRecomp (GTA IV port). C++20, Clang-cl + Ninja + vcpkg on Windows.

**Primary Recompiler: rexglue-sdk v0.8.0** (决定 2026-06-16)
- `refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe`
- 分析能力远超 XenonRecomp：310 导入，5.6M 指令，10K 代码区域
- 内置完整 Runtime（D3D12/Vulkan, Audio, Input, ImGui）
- **关键优势 vs XenonRecomp**:
  - 内联 MMIO 检查（load/store 宏直接判断地址范围，不依赖 access violation）
  - Xbox 360 SEH → C++ try/catch（结构化异常映射）
  - vtable 扫描 + RTTI 恢复虚函数调用
  - 原生 CRT hooks（malloc/memcpy 可重定向到原生实现）
  - 自动生成函数导出、异常处理、模板脚手架
- XenonRecomp 保留作为辅助工具

**Current Phase: rexglue 集成 — XEX 加载成功，游戏开始执行**
- ✅ rexglue 预编译 SDK v0.8.1.32 成功集成
- ✅ 197 PPC 文件编译 + 链接（200MB exe）
- ✅ Runtime::Setup() 成功
- ✅ LoadXexImage 通过 VFS `game:\default.xex` 路径成功
- ⚠️ 游戏加载后 segfault——需调试崩溃原因
- 🔧 exe 位置：`test_rexglue/out/v6/gta5_rexglue.exe`
- 🔧 game_data_root：`D:/Games/Xenia/gta5`（含 RPF 资产）
- 分支：`feature/rexglue-source-build`

**⚠️ 工作纪律（每次必读）：**
- **绝不主动停下来** — 除非遇到需要用户决策的硬阻塞
- **每阶段结束必须更新 CLAUDE.md + GAME_PLAN.md**
- **每次会话开始必须读取 CLAUDE.md + GAME_PLAN.md**
- **涉及代码修改的操作不要问我，直接做**

## Build Commands

All commands run from Git Bash at the project root.

```bash
# Full build (configure + compile + link)
bash build_sanrecomp.sh

# Manual configure
export VCPKG_ROOT="$(pwd)/thirdparty/vcpkg"
export PATH="/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin:/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:$PATH"
CMAKE="/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

"$CMAKE" -S "$(pwd)" -B out/build/x64-Clang-RelWithDebInfo -G Ninja \
  -DCMAKE_C_COMPILER=clang-cl.exe -DCMAKE_CXX_COMPILER=clang-cl.exe \
  -DCMAKE_LINKER=lld-link.exe -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  "-DCMAKE_LIBRARY_PATH=C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"

# Build a single target
"$CMAKE" --build out/build/x64-Clang-RelWithDebInfo --target SanRecompLib

# Regenerate PPC files from GTA V XEX
cd SanRecompLib
../tools/XenonRecomp/build/XenonRecomp/XenonRecomp.exe config/GTA5_quick.toml ../tools/XenonRecomp/XenonUtils/ppc_context.h
```

**Critical:** Always use the x64 clang-cl at `VC/Tools/Llvm/x64/bin/clang-cl.exe`, NOT the 32-bit one at `VC/Tools/Llvm/bin/clang-cl.exe`. The 32-bit version targets x86 by default and will cause architecture macro mismatch.

**Critical:** `VCPKG_ROOT` must be set before running cmake, or the configure step will fail.

## Architecture

```
GTA V default.xex (26MB PowerPC binary)
    ↓ XenonRecomp
546 ppc_recomp.*.cpp + PPCContext runtime
    ↓ compiled as SanRecompLib.lib
    ↓ linked with
SanRecomp.exe (kernel emulation + renderer + audio + input)
```

- **SanRecompLib**: PPC recompiled game code as static library (551 files, 100% compile)
- **SanRecomp**: Host runtime — kernel syscall translation, GPU command translation via plume, audio, HID, installer UI

## Key Files / Directories

| Path | Purpose | Status |
|------|---------|--------|
| `SanRecompLib/ppc/` | 546 generated PPC→C++ files from XenonRecomp | ✅ Generated, compiled |
| `SanRecompLib/config/GTA5.toml` | XenonRecomp config for GTA V | ⚠️ Uses GTA IV register addresses |
| `SanRecomp/kernel/` | Xbox 360 kernel translation layer | ✅ All files enabled |
| `SanRecomp/gpu/video.cpp` | D3D12/Vulkan renderer via plume (~7800 lines) | ⚠️ Enabled, pipeline #if 0'd on Intel |
| `SanRecomp/hid/` | Input handling (SDL + XInput) | ✅ All files enabled |
| `SanRecomp/ui/` | Installer wizard, ImGui UI, game window | ✅ All enabled except stubs |
| `SanRecomp/link_stubs.cpp` | Stubs for unbuilt libraries (FFmpeg, SDL_mixer, networking, etc.) | ⚠️ Required for link |
| `SanRecomp/api/` | RAGE engine reverse-engineered structures | ⚠️ Mixed GTA4/GTA5 namespaces |
| `tools/XenonRecomp/` | PPC→C++ recompiler tool | ✅ Built |
| `tools/XenosRecomp/` | Xenos shader→HLSL tool | ✅ Cloned, not yet run |
| `tools/file_to_c.cpp` | BIN2C converter for resource embedding | ✅ Built |
| `tools/bc_diff/bc_diff.h` | Resource declarations + BC diff types | ✅ Stub with all externs |
| `thirdparty/plume/` | Cross-platform GPU abstraction (D3D12, Vulkan, Metal) | ✅ Built as static lib |
| `thirdparty/SDL/` | SDL2 static lib | ✅ Built (release-2.30.9) |
| `thirdparty/imgui/` | Dear ImGui | ✅ v1.90.9 (detached) |
| `refs/` | 参考项目 (UnleashedRecomp, rexglue-sdk, etc.) | ✅ 已下载并研究 |
| `refs/rexglue-sdk/rexglue-bin/` | rexglue-sdk v0.8.0 预编译工具 | ✅ Windows 版本可用 |
| `refs/UnleashedRecomp/` | Sonic Unleashed 重编译参考（hedge-dev） | ✅ 完整工作项目 |
| `refs/rexglue-sdk/` | rexglue-sdk 源码 | ✅ v0.8.0, C++23 |

## Submodule Versions (Critical)

These were manually switched from defaults:

| Submodule | Expected | Current | Reason |
|-----------|----------|---------|--------|
| `thirdparty/SDL` | SDL2 (2.30.9) | `release-2.30.9` (detached) | Code uses SDL2 API |
| `thirdparty/SDL_mixer` | SDL2_mixer (2.8.2) | `release-2.8.2` (detached) | SDL2 compat, not yet built |
| `thirdparty/imgui` | v1.90.x | `v1.90.9` (detached) | Code uses older ImGui API |

**Do NOT** run `git submodule update` without specifying these versions or they will revert to SDL3/ImGui 1.92.

## Environment

- **OS**: Windows 11 x64
- **Shell**: Git Bash (MSYS2 from devkitPro at `C:\devkitPro\msys2`)
- **VS**: Visual Studio 2026 (v18.6.1) at `C:\Program Files\Microsoft Visual Studio\18\Community`
- **GPU**: Intel integrated (D3D12 — pipeline creation crashes on some calls)
- **GTA V files**: `D:\Games\Xenia\gta5\` (extracted), ISOs at `D:\Games\Xenia\Grand Theft Auto V...\`
- **IDA Pro**: `C:\Program Files\IDA Professional 9.3`

## Git State

- **Remote**: `origin` = `git@github.com:Je11yb0ne/SanRecomp.git` (fork of OZORDI/SanRecomp)
- **Upstream**: `git@github.com:OZORDI/SanRecomp.git`
- **Branch**: `feature/xenonrecomp-phase9` (active), `feature/rexglue-migration` (archived), `main` (old)
- **Backup tag**: `backup/pre-rexglue-migration` (clean XenonRecomp state)
- **Backup naming**: `claude-code/github-backup-YYYYMMDD-HHMMSS`
- **Push rule**: `git push origin <branch>` after each milestone. NEVER use `git add -A`. Use `git add .` from repo root.
- All submodules manually cloned (git submodule mechanism broken for this repo)
- Many untracked files from fixes; none committed to main yet

## Currently Disabled Files

Only these source files remain disabled in `SanRecomp/CMakeLists.txt`:

1. **gpu/imgui/imgui_font_builder.cpp** — needs msdfgen/msdfgen-config.h (cmake generate step)
2. **patches/player_limit_patches.cpp** — undeclared GTA V PPC function addresses (`sub_826A6CC8`, `sub_826AE738`)
3. **patches/aspect_ratio_patches.cpp** through **patches/video_patches.cpp** (~12 files) — Sonicteam (GTA IV) references, need GTA V rewrites

## Known Workarounds (Active)

1. **o1heap → malloc** (`kernel/heap.cpp`): o1heapAllocate crashes on 2GB physical heap. `AllocPhysical` uses `malloc()`. 失去确定性分配优势。
2. **D3D12 pipeline #if 0** (`gpu/video.cpp`): 完整管线（shaders, ImGui）被 `#if 0` 包裹。Intel D3D12 驱动在 `SetDescriptorHeaps`/`SetGraphicsRootDescriptorTable` 崩溃。
3. **InstallerWizard early return** (`ui/installer_wizard.cpp`): 立即返回 `true`，不渲染 UI。
4. **最小渲染模式** (`gpu/video.cpp`): `BeginCommandList` 在 `g_pipelineLayout==null` 时跳过 descriptor 绑定 + 直接渲染到 swap chain backbuffer（跳过中介纹理），避免 Intel D3D12 崩溃。
5. **PPC 看门狗** (`cpu/guest_thread.cpp`): 每秒打印 r1/r3 寄存器值，带 `#include <thread>` + `<atomic>`。

## Current Blockers (Phase 4)

1. **PPC 代码忙等** — 入口 0x83639888 可执行但不调用任何内核 stub。看门狗显示 r1 静态（卡在单个函数内），疑似在忙等 Xbox 360 硬件寄存器。rexglue 分析发现 GTA V 有 **310 个导入符号**，我们的项目只 stubbed ~50 个。
2. **Intel D3D12 管线** — 最小 clear+present 已工作 ✅。完整管线需要 descriptor heap 绑定 → 驱动崩溃。

## 诊断工具

- **PPC 看门狗**: `guest_thread.cpp` 中自动打印 r1/r3 寄存器（每秒）。`r1 变化` = 函数调用活跃；`r1 静态` = 卡在单个函数内。
- **内核调用追踪**: `link_stubs.cpp` 中 `#define KERNEL_TRACE` 启用/禁用所有 `__imp__*` stub 的 printf 追踪。
- **rexglue 分析**: `refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe init/codegen` 可重新分析 XEX，找到完整导入表。

## Known Fixes Applied

- **Root CMakeLists.txt**: Added `CMAKE_SIZEOF_VOID_P=8 FORCE` after `project()` (clang-cl doesn't auto-detect)
- **SanRecompLib/CMakeLists.txt**: Commented out XenonRecomp/XenosRecomp target_compile_definitions
- **SanRecomp/CMakeLists.txt**: Added plume subdirectory, enabled video.cpp + 4 UI source files, linked dxcompiler.lib
- **SanRecomp/kernel/xbox.h**: Replaced stub with XenonUtils official version (includes `be<T>`, `ByteSwap`)
- **SanRecomp/kernel/byteswap.h**: Copied from XenonUtils
- **SanRecomp/patches/gta5_*.cpp**: Created from gta4_* counterparts (content already GTA V)
- **SanRecomp/res/version.txt**: Created with alpha v0.1.0 version info
- **SanRecomp/res/win32/res.rc.template**: Commented out ICON line (placeholder ICO unsupported)
- **SanRecomp/stdafx.h**: Added `_USE_MATH_DEFINES`, `#include <winsock2.h>` before `<windows.h>`
- **SanRecomp/sdl_events.h**: Rewrote for SDL2 API (was SDL3 compat macros)
- **SanRecomp/cpu/guest_thread.h**: Added `GetPPCContext()`, `SetPPCContext()`, `g_ppcContext` declarations
- **SanRecomp/cpu/guest_thread.cpp**: Added `g_ppcContext` definition
- **SanRecomp/kernel/function.h**: Added `#include <cpu/guest_thread.h>`
- **SanRecomp/kernel/xdm.h**: Added `#include <xbox.h>`, XINPUT_KEYSTROKE `#ifndef` guards
- **SanRecomp/gpu/video.cpp**: SPIR-V includes guarded with `#ifndef _WIN32`, CREATE_SHADER macro uses DXIL only on Windows, ImPlot PlotLine API updated, createSwapChain uses RenderSwapChainDesc, pipeline in `#if 0`
- **SanRecomp/ui/game_window.cpp**: SetIcon uses `const void*`, SDL_RWFromMem with const_cast
- **SanRecomp/ui/installer_wizard.cpp**: Added `#include <bc_diff.h>`, early return when pipeline not ready
- **SanRecomp/kernel/heap.cpp**: AllocPhysical uses malloc instead of o1heapAllocate
- **SanRecompLib/config/GTA5.toml**: Updated paths to `../private/default.xex` and local switch tables
- **tools/XenonRecomp/XenonUtils/recompiler.cpp**: Bounds-check fix for bdz/bdnz/bdnzf instructions
- **tools/XenonRecomp/XenonUtils/image.h**: Added `resource_offset`, `resource_size` fields
- **tools/XenonRecomp/XenonUtils/xex_patcher.cpp**: Fixed `_BitScanForward64` → `__builtin_ctzll`
- **tools/XenonRecomp/XenonUtils/xex.cpp**: Compiled directly (not the install/xbox360 stripped version), BSS gap fix for basic compression
- **tools/bc_diff/bc_diff.h**: Created stub with all resource externs
- **tools/file_to_c.cpp**: Created BIN2C converter utility
- **tools/CMakeLists.txt**: Added file_to_c executable target
- **thirdparty/CMakeLists.txt**: Created (was missing, broke cmake configure)

## link_stubs.cpp Status

This file provides stubs for libraries not yet built. Current stub categories:
- **ZSTD**: decompress + error handling stubs
- **FFmpeg (avcodec)**: All return null/-1 (XMA audio decoding not functional)
- **SDL_mixer**: All stubbed (build dependencies complex)
- **GameNetworkingSockets**: Init/Kill stubs
- **NFD (Native File Dialog)**: All stubs
- **SoundTouch**: Audio processing stubs
- **DxcCreateInstance**: Resolved by linking dxcompiler.lib
- **PPC kernel stubs**: ~50 `__imp__*` Xbox 360 kernel exports (networking, XAM, etc.)
- **Resource data**: 20+ `g_*_uncompressed_size = 1` definitions
- **Shader cache**: Empty g_shaderCacheEntries, g_compressedSpirvCache/DxilCache
- **PlayerLimitPatches::Init**: Empty stub
- **_sub_829D1758, _sub_829D8860**: GTA V PPC function stubs

## rexglue-sdk (Primary Recompiler)

rexglue-sdk v0.8.0，C++23 + clang + Ninja，基于 Xenia 内核。

**Wiki:** `refs/rexglue-sdk-wiki/`（已 clone，必读）
**预编译工具:** `refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe`
**生成代码:** `refs/rexglue-sdk/rexglue-bin/win-amd64/test_gta5/generated/default/`

### 核心架构（从 Wiki 学习）
- **Runtime**: 中央管理器，初始化顺序：Memory → ExportResolver → FunctionDispatcher → VFS → KernelState → Graphics/Audio/Input
- **FunctionDispatcher**: guest PPC addr → host fn 指针的映射表，位于 PPC 内存 `IMAGE_BASE + IMAGE_SIZE`，索引 = `(addr - CODE_BASE) * 2`
- **弱别名机制**: 每个重编译函数是 `__imp__name` 的弱别名。用 `REX_HOOK_RAW(name)` 可覆盖，`__imp__name` 总指向原始实现
- **REX_HOOK_RAW**: `extern "C" void name(PPCContext& ctx, uint8_t* base)`，等价我们的强符号覆盖
- **REX_STUB**: 日志+空操作 stub，`REX_STUB_RETURN(name, val)` 设置 r3 返回值
- **KernelState**: 管理 3 个进程(Idle/System/Title)，对象表，线程注册，TLS，DPC
- **模块加载**: 只加载 XEX 数据段(不加载代码)，代码已在编译时链接
- **ReXCRT**: 可将 CRT 函数映射到 host 原生实现（malloc/memcpy 等）
- **SEH**: `SEH_TRY/SEH_CATCH_ALL/SEH_RETHROW` 支持 PPC 异常处理

### 与 XenonRecomp 关键差异
| 特性 | XenonRecomp | rexglue |
|---|---|---|
| 弱别名 | 手动 `__attribute__((alias))` | `DEFINE_REX_FUNC` 宏自动生成 |
| 覆盖机制 | 手动强符号定义 | `REX_HOOK`/`REX_HOOK_RAW`/`REX_STUB` |
| 函数表 | `PPC_LOOKUP_FUNC` 宏 | `FunctionDispatcher` 类 |
| 内核 | imports.cpp (~190 hooks) | KernelState + 完整 xboxkrnl/xam 实现 |
| 内存 | VirtualAlloc 手动管理 | rex::Memory 子系统 |
| 线程 | GuestThreadContext 手动 | XThread + KernelState::LaunchModule |
| CRT | 无 | ReXCRT 映射（malloc→原生） |

### rexglue 集成路径
1. `rexglue init` → 生成项目脚手架
2. `rexglue codegen manifest.toml` → 生成 193 .cpp + init.h/cpp
3. CMakeLists.txt 中 `include(generated/rexglue.cmake)` + `rexglue_setup_target()`
4. 入口: `rex::Runtime::Setup()` → `LaunchModule()` → `XThread` 启动重编译的入口函数
5. 内核函数用 `REX_EXPORT(name, impl)` 注册，游戏函数用 `REX_HOOK_RAW(name)` 覆盖

### 命令行
```bash
REXGLUE="refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe"
"$REXGLUE" init --app_name "GTA5" --app_root ./GTA5
"$REXGLUE" codegen gta5_config.toml -v --force  # --force 跳过未解析调用
```

## Current State (2026-06-19)

**分支**: `feature/rexglue-source-build`

### 🎉 RPF7 设备 + 6 归档合并 → 真实资产加载 + 游戏呈现真实帧！

| 里程碑 | 状态 |
|--------|------|
| RpfContainerDevice (RPF7 设备) | ✅ `refs/rexglue-sdk/src/filesystem/devices/rpf_container_device.{h,cpp}` |
| 解码验证 (AES-256单次 + 分块LZX) | ✅ common.rpf branches.meta → 合法 XML（设备确切代码路径二次验证）|
| VFS overlay (最长前缀+并集) | ✅ `virtual_file_system.cpp::ResolvePath` |
| 6 归档合并 @ game:\xbox360\ | ✅ xbox360a/b + install\part0-3.rpf (1451 条目) |
| 8GB 安装提取 | ✅ `stfs_extract` STFS→`D:\Games\Xenia\gta5\install\part{0-3}.rpf` |
| 真实资产加载 | ✅ scaleform/levels/models/streamedpeds 等从合并归档解析 |
| **游戏呈现真实帧** | ✅ **GPU PRESENT 1280x720 format=6，循环 3 swap texture（三缓冲）+ 输入轮询** |

**关键架构**：
1. **STFS 含 partN.rpf（RPF7 归档）**，不是 loose 树。`stfs_extract --list` 确认根 = `partN.rpf` + `partN.timestamp`。需提取 partN.rpf 再用 RpfContainerDevice 解析。
2. **挂载链**：`game:\xbox360\<dir>` → game:→Partition1 → 符号链接 `\Device\Harddisk0\Partition1\xbox360\`（**必须尾分隔符**，否则吞 xbox360a.rpf）→ `\Device\RpfXbox360\`。设备合并 6 归档。`common:` → game root（游戏自挂 common.rpf）。`xbox360a/b.rpf` 文件本身经 host 解析（游戏自挂 packfile）。
3. **资源 RSC7 重建**：资源条目服务为 `[16B RSC7头(magic/version/sysFlags/gfxFlags 大端)] + 压缩体`；非资源解压后服务；`#→x` 在 ResolvePath 回退。
4. **AES**：bundled tiny-aes（AES256+ECB，public 符号重命名 `RpfAes_*` 防冲突）；**LZX**：bundled cabextract（分块）。密钥从 `<game_root>/(360)key.dat` 运行时读取（不入 git）。

### 🔴 下一阶段：引擎推进到可见 logo/菜单（非文件问题）

- 游戏**已过资产门槛**：真实资产加载，~13 个真实缺失文件（creaturemetadata/occlusion/scaleform_generic 等，盘+安装都没有）是**可选探测**（每个请求 1 次，游戏容忍）。
- 游戏**存活 + 呈现真实帧**（GPU PRESENT 1280x720 三缓冲 + XamInputGetCapabilities 轮询）。
- **需用户目视确认窗口**：游戏在呈现真实 swap texture，窗口应显示 logo/加载/菜单。若窗口仍黑但 GPU 在 PRESENT 真实纹理 → 查 plume swapchain 呈现链；若显示内容 → 目标达成。
- **rexglue DLL 重建**（改 src 后）：
  ```bash
  export VSROOT="/c/Program Files/Microsoft Visual Studio/18/Community"
  export PATH="$VSROOT/VC/Tools/Llvm/x64/bin:/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64:$VSROOT/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:$VSROOT/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin:$PATH"
  cd refs/rexglue-sdk/out/build/vulkan && ninja rexruntimerd.dll
  cp refs/rexglue-sdk/out/win-amd64/rexruntimerd.dll test_rexglue/out/easy/
  ```
- **跑游戏**：`cd test_rexglue/out/easy && ./gta5_rexglue.exe`（窗口 GUI；日志 `gta5_kernel.log`）。

---

## Current State (2026-06-18)

**分支**: `feature/rexglue-source-build`

### 重大突破：rexglue Vulkan 窗口 + 游戏运行稳定！

| 里程碑 | 状态 |
|--------|------|
| rexglue self-built DLL (Vulkan-only) | ✅ `refs/rexglue-sdk/out/win-amd64/rexruntimerd.dll` |
| 197 PPC 文件编译+链接 | ✅ `test_rexglue/out/easy/gta5_rexglue.exe` (343MB) |
| Runtime::Setup() 成功 | ✅ graphics=YES |
| Vulkan Presenter 创建 | ✅ presenter=YES（关键：必须在 Setup 前设置 app_context） |
| Win32 窗口弹出 | ✅ 窗口可见 |
| XEX 加载 + LaunchModule | ✅ 游戏启动 |
| VEH 页面错误处理 | ✅ AddVectoredExceptionHandler 让游戏稳定运行 15+ 秒 |
| 游戏渲染画面 | ⚠️ 窗口黑屏 — 游戏可能未提交绘制命令或 swapchain 未正确呈现 |

### 关键架构发现

1. **必须在 Setup 前设置 app_context**：`runtime.set_app_context(&app_context)` → `runtime.Setup()` → `SetupPresentation` 在内部被调用，Presenter 才能创建。
2. **禁止手动 VirtualAlloc GPU MMIO**：`VirtualAlloc(mem + 0x7FC80000, ...)` 会与 GPU 驱动冲突 → **蓝屏死机**。rexglue 内部处理 MMIO。
3. **VEH 页面错误处理必要**：游戏访问未提交内存时，`AddVectoredExceptionHandler` 自动 `VirtualAlloc` 提交页面，游戏才能继续运行。

### 当前链路

```
main() → Runtime() → set_app_context() → Setup(Vulkan) → Window::Create → SetPresenter
→ LoadXexImage → LaunchModule → RunMainMessageLoop
```

### 工作目录

- **测试项目**: `test_rexglue/` (独立 CMake 项目)
- **构建目录**: `test_rexglue/out/easy/`（已验证配置，不要改 CMakeLists.txt）
- **DLL 来源**: `refs/rexglue-sdk/out/win-amd64/rexruntimerd.dll` (self-built Vulkan-only)
- **预编译 SDK 头文件**: `tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64/include/`
- **预编译 SDK 库**: `tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64/lib/`

### 构建命令

```bash
export VSROOT="/c/Program Files/Microsoft Visual Studio/18/Community"
export LLVM="$VSROOT/VC/Tools/Llvm/x64/bin"
export NINJA_DIR="$VSROOT/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
export SDK_BIN="/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
export CMAKE_DIR="$VSROOT/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
export PATH="$LLVM:$SDK_BIN:$NINJA_DIR:$CMAKE_DIR:$PATH"
cd test_rexglue/out/easy && cmake --build . -j 1
```

### 下一步

1. **诊断黑屏** — 游戏是否调用 VdSwap？GPU 命令缓冲是否有内容？
2. **参考 TDURE 完整工作项目** — 对比渲染管线差异
3. **添加 VdSwap 追踪** — 确认游戏渲染循环是否活跃
