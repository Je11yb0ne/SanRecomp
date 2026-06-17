// GTA V — rexglue runtime with WIN32 window
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/rex_app.h>
#include <rex/ui/windowed_app_context_win.h>
#include <rex/system/xthread.h>
#include <cstdio>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#include <windows.h>
static LONG WINAPI PageFaultHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t addr = info->ExceptionRecord->ExceptionInformation[1];
        if (VirtualAlloc((LPVOID)(addr & ~0xFFFULL), 0x1000, MEM_COMMIT, PAGE_READWRITE))
            return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
#ifdef _WIN32
    AddVectoredExceptionHandler(1, PageFaultHandler);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";

    rex::Runtime runtime(gameRoot, userRoot);
    rex::ui::Win32WindowedAppContext appCtx(hInstance, nCmdShow);
    if (!appCtx.Initialize()) { printf("AppContext init failed\n"); return 1; }
    runtime.set_app_context(&appCtx);

    printf("Setting up runtime with GPU...\n");
    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{});
    if (status != 0) { printf("Setup failed: 0x%08X\n", (unsigned)status); return 1; }
    printf("Runtime OK. Loading XEX...\n");

    status = runtime.LoadXexImage("game:\\default.xex");
    if (status != 0) { printf("XEX load failed: 0x%08X\n", (unsigned)status); return 1; }
    printf("Launching...\n");

    auto thread = runtime.LaunchModule();
    printf("Running. Close window to exit.\n");

    int result = appCtx.RunMainMessageLoop();
    CoUninitialize();
    return result;
}
