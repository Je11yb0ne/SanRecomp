// GTA V — RexGlue Recompiled Application
// Pre-built rexruntimed.dll (v0.8.1.32) doesn't export some ReXApp virtuals.
// We provide minimal stubs until source build is available.
#pragma once

#include <rex/rex_app.h>
#include "generated/default/gta5_recomp_init.h"

class GTA5App final : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;
    ~GTA5App() {}

    static std::unique_ptr<rex::ui::WindowedApp> Create(
        rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<GTA5App>(
            new GTA5App(ctx, "GTA5", PPCImageConfig));
    }

protected:
    // Pre-built DLL doesn't export these — provide stubs
    bool SetupEnvironment() override { return true; }
    bool ConstructRuntime(const rex::PathConfig&) override { return true; }
    bool SetupPresentation() override { return true; }
    void LaunchModule() override {}
    bool OnInitialize() override { return true; }
    void OnDestroy() override {}

public:
    void OnKeyDown(rex::ui::KeyEvent&) override {}
    void OnKeyUp(rex::ui::KeyEvent&) override {}
    void OnClosing(rex::ui::UIEvent&) override {}
};
