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

**Current Phase: 4 — Rendering Pipeline + Game Boot.** SanRecomp.exe compiles, links (0 errors), and runs. D3D12 device + swap chain created. SDL window shows "San Recompiled" title. PPC code boots via GuestThread::Start (entry=0x83639888). **Blocker:** Full D3D12 pipeline (shaders, ImGui rendering) disabled on Intel GPU — window is pure black.

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

These are NOT fixes — they're temporary bypasses that need proper solutions:

1. **o1heap → malloc** (`kernel/heap.cpp`): o1heapAllocate crashes with access violation on 2GB physical heap. `AllocPhysical` uses `malloc()` instead. Lost benefit of deterministic allocation.
2. **D3D12 pipeline #if 0** (`gpu/video.cpp:2292-2542`): Full pipeline setup (shaders, ImGui, render passes) wrapped in `#if 0` because Intel D3D12 driver crashes in `getSampleCountsSupported`, `createCommandQueue(COPY)`, and pipeline creation.
3. **InstallerWizard early return** (`ui/installer_wizard.cpp`): `InstallerWizard::Run` returns `true` immediately without rendering UI (pipeline not ready).

## Current Blockers (Phase 4)

1. **Intel D3D12 pipeline crashes** — Root cause: Intel GPU driver issues with specific D3D12 API calls. Need to isolate which calls crash and find alternatives or fallbacks.
2. **Black window** — Window exists but no content rendered because full pipeline is skipped. Need minimal clear+present to prove rendering works.
3. **PPC code hangs** — PPC code executes but eventually hangs (exit 124 = timeout). Need printf-based tracing to see where it gets stuck.

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

## Next Session Starting Points

1. **Minimal D3D12 clear+present** — Add a simple clear color + Present() before the #if 0 block to prove rendering works and show something other than black
2. **Isolate Intel D3D12 crash** — Test individual pipeline creation calls with SEH to find which specific call crashes
3. **PPC execution tracing** — Add printf in GuestThread::Start loop to see what PPC code does and where it hangs
4. **Build SDL_mixer** — Configure SDL_mixer cmake build to remove stubs
5. **Run XenosRecomp** — Convert Xbox 360 .fxc shaders to DXIL for real shader pipeline
6. **Fix imgui_font_builder.cpp** — Generate msdfgen-config.h via cmake
