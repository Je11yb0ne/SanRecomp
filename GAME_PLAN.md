# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that boots the game.

**Rule:** After each phase, update this file. Never deviate from the current phase. Never ask "continue?" — just work.

**Current Phase:** Phase 3 — Rendering + UI Active ✅ IN PROGRESS

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
- ⚠️ Full pipeline skipped for Intel GPU (crashes in getSampleCountsSupported/createCommandQueue)

## Phase 4: Rendering Pipeline + Game Boot (NEXT)
**Goal:** Fix D3D12 pipeline crashes on Intel, complete game boot
**Blockers:** Intel D3D12 driver issues, PPC code hangs without output, GitHub push permission

**⚠️ Self-Audit Checklist — Run Before Each Work Session:**
1. Read CLAUDE.md
2. `Skill("using-superpowers")` then `Skill("andrej-karpathy-skills:karpathy-guidelines")`
3. Read this GAME_PLAN.md
4. Apply systematic-debugging: batch ALL errors before fixing ANY
5. Surgical changes only — Edit tool, not sed/awk

---

## Phase Progress Log

### 2026-06-15 (update 2)
- ✅ `memory_mapped_file.cpp` added from XenonUtils → 7 MemoryMappedFile symbols resolved
- 🔍 20 remaining: 5 imports.cpp kernel hooks + 14 disabled UI + 1 Xex2LoadImage
- **Next:** Re-enable imports.cpp (5 kernel symbols)

## Phase 1: Complete the Link

**Success criterion:** `cmake --build ... --target SanRecomp` produces `SanRecomp.exe` with **zero** compile errors and **zero** linker errors.

**Starting state (2026-06-15 evening):** 0 compile errors, ~20 linker errors. All errors are from disabled source files. The stub approach (`link_stubs.cpp`) has run its course.

### Mainline

Re-enable disabled source files in this order, fixing each before moving to the next.

#### 1.1 Re-enable `kernel/mapped_file.cpp`

**Why first:** Provides `MemoryMappedFile` class (7 linker symbols). This file likely compiles cleanly — it just needs to be in the source list.

**Steps:**
```
1. In SanRecomp/CMakeLists.txt, change:
     # TODO-FIX: "kernel/mapped_file.cpp"
   To:
     "kernel/mapped_file.cpp"
2. Reconfigure + build: bash build_sanrecomp.sh
3. If compile errors: fix them (likely missing includes)
4. If link OK: remove corresponding stubs from link_stubs.cpp
5. Mark done below
```

**Done:** [ ]

#### 1.2 Re-enable `kernel/imports.cpp`

**Why:** Largest kernel file (~18K lines). Provides `InitKernelMainThread`, `PumpSdlEventsIfNeeded`, `XINPUT_KEYSTROKE` handlers, many kernel exports.

**Known issues:**
- `STATUS_*` macros conflict with Windows `<ntstatus.h>`. Fix: added `#undef` before local definitions.
- `XRTL_CRITICAL_SECTION` members use plain `int32_t` but code calls `.get()`. Fix: already applied sed replacements.
- `XINPUT_KEYSTROKE` struct conflict. Fix: xdm.h already has guards.

**Steps:**
```
1. Uncomment in CMakeLists.txt
2. Build. If compile errors:
   a. For STATUS_* conflicts: ensure #undef lines are before the namespace block
   b. For XINPUT_KEYSTROKE: check Windows xinput.h vs xdm.h
3. Once compiles, remove corresponding stubs from link_stubs.cpp
4. Mark done
```

**Done:** [ ]

#### 1.3 Re-enable `ui/game_window.cpp`

**Known issues:** `SetIcon` function — may need `SDL_SetWindowIcon` or removal.

**Done:** [ ]

#### 1.4 Re-enable `gpu/video.cpp`

**Known issues:** SPIR-V includes (already commented out). SDL2 API calls.

**Done:** [ ]

#### 1.5 Re-enable `ui/installer_wizard.cpp`

**Known issues:** `g_*_uncompressed_size` resource variables. These come from BIN2C-generated headers.

**Done:** [ ]

#### 1.6 Re-enable `ui/imgui_utils.cpp`

**Known issues:** ImGui API version compatibility (v1.90.9), `g_*_uncompressed_size`.

**Done:** [ ]

#### 1.7 Re-enable `ui/achievement_menu.cpp`

**Known issues:** `g_trophy_uncompressed_size`.

**Done:** [ ]

#### 1.8 Re-enable `gpu/imgui/imgui_snapshot.cpp`

**Known issues:** ImGui API compatibility.

**Done:** [ ]

#### 1.9 Re-enable `gpu/imgui/imgui_font_builder.cpp`

**Known issues:** `msdfgen/msdfgen-config.h` needs cmake generate.

**Done:** [ ]

#### 1.10 Re-enable `patches/player_limit_patches.cpp`

**Known issues:** `sub_826A6CC8`, `sub_826AE738` — need PPC stubs.

**Done:** [ ]

### Cleanup step

Once ALL files compile and link:
1. Remove `link_stubs.cpp` — it should be empty by now
2. Remove the duplicate `"install/installer.cpp"` line if not already done
3. Verify: `SanRecomp.exe` produced at `out/build/x64-Clang-RelWithDebInfo/SanRecomp/SanRecomp.exe`

---

## Phase 2: First Launch Test

**Success criterion:** SanRecomp.exe starts without crashing immediately.

**Starting state:** SanRecomp.exe linked, all source files enabled.

1. Copy game files to expected location
2. Run `SanRecomp.exe`
3. Fix any crash-at-startup issues
4. Goal: reach the installer or main menu

---

## Phase 3: Functional Verification

**Success criterion:** Game boots, installer works, menu accessible.

1. Fix crash issues as they appear
2. Enable audio if possible
3. Enable rendering if possible

---

## Build Commands Reference

```bash
# Full configure + build (run from project root)
bash build_sanrecomp.sh

# Quick build (if cmake already configured)
export VCPKG_ROOT="$(pwd)/thirdparty/vcpkg"
export PATH="/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin:/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:$PATH"
CMAKE="/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
"$CMAKE" -S "$(pwd)" -B "out/build/x64-Clang-RelWithDebInfo" -G Ninja \
  -DCMAKE_C_COMPILER=clang-cl.exe -DCMAKE_CXX_COMPILER=clang-cl.exe \
  -DCMAKE_LINKER=lld-link.exe -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  "-DCMAKE_LIBRARY_PATH=C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
"$CMAKE" --build "out/build/x64-Clang-RelWithDebInfo" --target SanRecomp
```

## Phase Progress Log

### 2026-06-15 (initial)
- ✅ SanRecompLib: 551/551 files compile
- ✅ All submodules cloned + SDL2/ImGui versions set
- ✅ CMake configure: zero errors
- ✅ All C++ files compile: zero errors
- ⏳ Link: 20 undefined symbols remaining (all from disabled files)
- Created: link_stubs.cpp with external library stubs
- All `_sub_*` GTA IV PPC addresses stubbed in save_hooks.cpp, game_init.cpp, audio_patches.cpp
- SanRecompLib linked
- CURL linked from vcpkg
- SanRecompResources: placeholder files created
- xdbf_wrapper.cpp added to sources
