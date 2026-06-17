// GTA V — RexGlue Recompiled Application
// Minimal app class following TDURE pattern

#pragma once

#include <rex/rex_app.h>

// Include the generated GTA V code (PPCImageConfig + PPCFuncMappings)
#include "generated/default/gta5_recomp_init.h"

class GTA5App : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;

    static std::unique_ptr<rex::ui::WindowedApp> Create(
        rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<GTA5App>(
            new GTA5App(ctx, "GTA5", PPCImageConfig));
    }

    // Override hooks as needed for GTA V customization:
    // void OnPreSetup(rex::RuntimeConfig& config) override {}
    // void OnPostSetup() override {}
    // void OnShutdown() override {}
};
