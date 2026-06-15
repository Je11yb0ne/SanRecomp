# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⚠️ READ THIS FIRST — Every Session

Before doing ANY work, invoke these skills in order:

1. **`Skill("using-superpowers")`** — loads the toolset
2. **`Skill("andrej-karpathy-skills:karpathy-guidelines")`** — behavioral rules, DO NOT SKIP
3. **Read this entire file** — critical project context

Then at every step:
- **Think before coding.** State assumptions. Surface tradeoffs. No guessing.
- **Systematic debugging** — batch ALL errors before fixing one.
- **Surgical changes** — no blind `sed`/`awk`. Use Edit tool with exact text.
- **Simplicity first** — minimum code. No speculative changes.
- **End every session** by updating this file with progress and next starting point.

---

## Project Overview

SanRecomp is a static recompilation project porting GTA V (Xbox 360) to PC. It's a fork of LibertyRecomp (GTA IV port) in active migration. The codebase is C++20, built with Clang-cl + Ninja + vcpkg on Windows.

**Current Phase:** Build chain nearly complete. Compilation passes for core code; 20 linker symbols remain (all from disabled files + missing SDL2 lib).

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

## Key Files / Directories

| Path | Purpose | Status |
|------|---------|--------|
| `SanRecompLib/ppc/` | 546 generated PPC→C++ files from XenonRecomp | ✅ Generated, compiled |
| `SanRecompLib/config/GTA5.toml` | XenonRecomp config for GTA V | ⚠️ Uses GTA IV register addresses |
| `SanRecomp/kernel/` | Xbox 360 kernel translation layer | ⚠️ Some files disabled |
| `SanRecomp/gpu/` | D3D12/Vulkan renderer via plume | ❌ Disabled (SDL3→SDL2 issues, SPIR-V) |
| `SanRecomp/hid/` | Input handling (SDL + XInput) | ❌ Disabled (XINPUT_KEYSTROKE conflict) |
| `SanRecomp/api/` | RAGE engine reverse-engineered structures | ⚠️ Mixed GTA4/GTA5 namespaces |
| `tools/XenonRecomp/` | PPC→C++ recompiler tool | ✅ Built |
| `tools/XenosRecomp/` | Xenos shader→HLSL tool | ✅ Cloned |
| `thirdparty/` | External deps (SDL2, imgui v1.90.9, plume, etc.) | ✅ |

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
- **SanRecomp**: Host runtime — kernel syscall translation, GPU command translation, audio, HID, installer UI

## Submodule Versions (Critical)

These were manually switched from defaults:

| Submodule | Expected | Current | Reason |
|-----------|----------|---------|--------|
| `thirdparty/SDL` | SDL2 (2.30.9) | `release-2.30.9` (detached) | Code uses SDL2 API |
| `thirdparty/SDL_mixer` | SDL2_mixer (2.8.2) | `release-2.8.2` (detached) | SDL2 compat |
| `thirdparty/imgui` | v1.90.x | `v1.90.9` (detached) | Code uses older ImGui API |

**Do NOT** run `git submodule update` without specifying these versions or they will revert to SDL3/ImGui 1.92.

## Environment

- **OS**: Windows 11 x64
- **Shell**: Git Bash (MSYS2 from devkitPro at `C:\devkitPro\msys2`)
- **VS**: Visual Studio 2026 (v18.6.1) at `C:\Program Files\Microsoft Visual Studio\18\Community`
- **GTA V files**: `D:\Games\Xenia\gta5\` (extracted), ISOs at `D:\Games\Xenia\Grand Theft Auto V...\`
- **IDA Pro**: `C:\Program Files\IDA Professional 9.3`

## Current Disabled Files (TODO-FIX)

These source files are commented out in `SanRecomp/CMakeLists.txt` with `# TODO-FIX:` prefix. Re-enabling them is the next step:

1. **kernel/imports.cpp** — STATUS_ macros conflict, XINPUT_KEYSTROKE type, XRTL_CRITICAL_SECTION .get() calls
2. **hid/driver/sdl_hid.cpp** — XINPUT_KEYSTROKE_* macros (xdm.h defines exist but Windows conflict)
3. **gpu/video.cpp** — SPIR-V includes need `#ifndef _WIN32` guard, SDL2 API calls
4. **ui/installer_wizard.cpp** — `g_*_uncompressed_size` resource variables (need BIN2C stubs)
5. **ui/imgui_utils.cpp** — `g_*_uncompressed_size` + ImGui API version issues
6. **gpu/imgui/imgui_snapshot.cpp** — ImGui API version mismatch
7. **gpu/imgui/imgui_font_builder.cpp** — msdfgen/msdfgen-config.h (needs cmake generate)
8. **ui/game_window.cpp** — SetIcon SDL2 API
9. **patches/player_limit_patches.cpp** — undeclared GTA V PPC function addresses
10. **ui/achievement_menu.cpp** — `g_trophy_uncompressed_size`

## Known Fixes Applied

- **Root CMakeLists.txt**: Added `CMAKE_SIZEOF_VOID_P=8 FORCE` after `project()` (clang-cl doesn't auto-detect)
- **SanRecompLib/CMakeLists.txt**: Commented out XenonRecomp/XenosRecomp target_compile_definitions (tools not built as cmake targets)
- **SanRecomp/CMakeLists.txt**: Added kernel, XenonUtils, SIMDe, toml++, zstd, o1heap, msdfgen, ffmpeg, vcpkg curl include paths
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
- **SanRecompLib/config/GTA5.toml**: Updated paths to `../private/default.xex` and local switch tables
- **tools/XenonRecomp/XenonUtils/recompiler.cpp**: Bounds-check fix for bdz/bdnz/bdnzf instructions
- **tools/XenonRecomp/XenonUtils/image.h**: Added `resource_offset`, `resource_size` fields
- **tools/XenonRecomp/XenonUtils/xex_patcher.cpp**: Fixed `_BitScanForward64` → `__builtin_ctzll`
- **tools/bc_diff/bc_diff.h**: Created stub with all resource externs
- **tools/file_to_c.cpp**: Created BIN2C converter utility
- **tools/CMakeLists.txt**: Added file_to_c executable target

## Git State

- Branch: `main` (clean, no upstream commits)
- All submodules manually cloned (git submodule mechanism broken for this repo)
- Many untracked files from our fixes; none committed yet
- SanRecompResources is locally populated with placeholder files

## Current Status (2026-06-15)

**Session achievement: SanRecomp compiles and links with 20 remaining undefined symbols.**

### Working ✅
- XenonRecomp built and generates 546 PPC files from GTA V default.xex
- SanRecompLib (PPC library): 551/551 files, 100% compile
- SDL2 (release-2.30.9) and SDL_mixer (release-2.8.2) submodules switched
- Dear ImGui v1.90.9 compatible
- CMake configure + generate: zero errors
- Core source files (kernel, patches, install, user, mod, utils, locale, OS, CPU, APU, HID): compiles
- o1heap, xxHash, fmt, image.cpp: compiled and linked

### Remaining Linker Symbols (20)
- 6 `_sub_*` PPC functions — need dllexport fix in SanRecompLib build config
- 4 SDL functions — need SDL2 compiled as static lib or DLL linked
- 2 Video (CreateHostDevice, WaitOnSwapChain) — video.cpp disabled
- 2 GameWindow (Update, GetTitle) — game_window.cpp disabled
- 1 InstallerWizard::Run — installer_wizard.cpp disabled
- 1 XDBFWrapper — definition missing
- 1 InitKernelMainThread — imports.cpp disabled
- 1 PPCFuncMappings — SanRecompLib not linked
- 1 GameNetworkingSockets_Kill — disabled

### Next Session Starting Points
1. Fix SanRecompLib DLL import/export → change `__declspec(dllimport)` to regular functions in ppc_config.h
2. Build SDL2 as static library or download prebuilt SDL2-devel .lib
3. Re-enable video.cpp (fix SPIR-V includes + SDL API)
4. Fix XDBFWrapper definition
5. Re-enable game_window.cpp (fix SetIcon)
6. Re-enable installer_wizard.cpp (fix _uncompressed_size resources)
7. Re-enable imports.cpp (fix STATUS_ macros and XRTL_CRITICAL_SECTION)
