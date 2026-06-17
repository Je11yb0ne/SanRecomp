// GTA V — RexGlue Recompiled Application
// Entry point following TDURE's clean pattern
//
// The rexglue runtime (rexruntime.dll) provides:
//   - Full Xbox 360 kernel emulation (Ke*, Nt*, Vd*, Xam*, etc.)
//   - GPU MMIO interception + PM4 → D3D12/Vulkan translation
//   - Audio (FFmpeg XMA decoding), Input (SDL3), UI (ImGui)
//   - Memory management, threading, file I/O

#include "../SanRecomp/gta5_app.h"

REX_DEFINE_APP(gta5, GTA5App::Create)
