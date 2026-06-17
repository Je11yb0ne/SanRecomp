// GTA V — RexGlue Recompiled Application
// Minimal entry point following TDURE's pattern
//
// This replaces the entire SanRecomp runtime (imports.cpp, video.cpp, etc.)
// with rexglue's built-in kernel, graphics, audio, and input subsystems.

#include "gta5_app.h"

REX_DEFINE_APP(gta5, GTA5App::Create)
