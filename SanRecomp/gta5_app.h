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

protected:
    // Required virtual methods (pre-built lib doesn't export defaults)
    bool SetupPresentation() override {
        // Let rexglue handle graphics backend setup
        return true;
    }
    void LaunchModule() override {
        // rexglue launches the recompiled PPC entry point
        // PPCFuncMappings[entry=0x83639888] is called automatically
    }
    bool OnInitialize() override { return true; }

public:
    void OnKeyDown(rex::ui::KeyEvent&) override {}
    void OnKeyUp(rex::ui::KeyEvent&) override {}
    void OnClosing(rex::ui::UIEvent&) override {}
};
