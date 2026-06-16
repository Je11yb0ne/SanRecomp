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

SanRecomp is a static recompilation project porting GTA V (Xbox 360) to PC. It's a fork of LibertyRecomp (GTA IV port) in active migration. The codebase is C++20, built with Clang-cl + Ninja + vcpkg on Windows.

**Current Phase: 4 — Rendering Pipeline + Game Boot.** SanRecomp.exe compiles, links (0 errors), and runs. D3D12 device + swap chain work. **Minimal clear+present shows purple window.** PPC code boots via GuestThread::Start (entry=0x83639888) but hangs in a tight loop — no kernel stubs called, stack static. **Blocker:** PPC code busy-waits on unknown condition (疑似等待 Xbox 360 硬件寄存器).

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
- **Branch**: `main` (local), backup branches at `claude-code/github-backup-*`
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

## rexglue-sdk (Alternative Recompiler)

rexglue-sdk v0.8.0 is a newer recompilation framework (C++23, clang, Ninja). Pre-built binary at `refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe`.

```bash
# Initialize project from XEX
REXGLUE="refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe"
"$REXGLUE" init --project-name "GTA5" --xex-path "path/to/default.xex" --game-root "path/to/game"

# Generate recompiled code (SLOW - 5.6M instructions)
"$REXGLUE" codegen manifest.toml -v
```

Key differences from XenonRecomp:
- Finds 310 import symbols (vs our ~50 stubs)
- 10,728 code regions (vs ~546 files)
- Built-in runtime (rex::Runtime, D3D12/Vulkan, audio, input)
- No game_init.cpp needed — PPC code runs directly

## Next Session Starting Points

1. **完成 rexglue codegen** — 让 rexglue 完整生成 GTA V 重编译代码，对比分析差异
2. **补全 310 个导入符号** — 根据 rexglue 发现的导入表或 IDA 分析，补全缺失的 ~260 个 kernel stubs
3. **PPC 忙等定位** — 用 IDA 反编译入口点 0x83639888，找到 busy-wait 的内存地址，在 Host 端伪造值
4. **对比 UnleashedRecomp 架构** — 参考其无 game_init.cpp 模式，评估是否移除 host 端初始化干预
5. **Build SDL_mixer** — 配置 SDL_mixer cmake 构建，移除 stubs
6. **Run XenosRecomp** — 转换 Xbox 360 .fxc shaders 到 DXIL
