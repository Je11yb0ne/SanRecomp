// GTA V — RexGlue Recompiled Application (TDURE pattern)
#pragma once

#include <rex/rex_app.h>
#include "generated/default/gta5_recomp_init.h"

class GTA5App : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;

    static std::unique_ptr<rex::ui::WindowedApp> Create(
        rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<GTA5App>(
            new GTA5App(ctx, "GTA5", PPCImageConfig));
    }
};
