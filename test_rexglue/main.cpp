// GTA V — WIN32 app with file-based logging
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/ui/windowed_app_context_win.h>
#include <rex/system/xthread.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

std::ofstream g_log;

#ifdef _WIN32
#include <windows.h>
static LONG WINAPI PageFaultHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t addr = info->ExceptionRecord->ExceptionInformation[1];
        if (VirtualAlloc((LPVOID)(addr & ~0xFFFULL), 0x1000, MEM_COMMIT, PAGE_READWRITE)) {
            static int n = 0;
            if (++n <= 20) g_log << "[MEM] committed " << (void*)(addr & ~0xFFFULL) << std::endl;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        g_log << "[FATAL] ACCESS_VIOLATION at 0x" << std::hex << addr << std::endl;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    g_log.open("gta5_rexglue.log");
    g_log << "=== GTA V rexglue WIN32 ===" << std::endl;

#ifdef _WIN32
    AddVectoredExceptionHandler(1, PageFaultHandler);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";
    rex::Runtime runtime(gameRoot, userRoot);

    rex::ui::Win32WindowedAppContext appCtx(hInst, nShow);
    if (!appCtx.Initialize()) { g_log << "FATAL: AppContext::Initialize failed" << std::endl; return 1; }
    runtime.set_app_context(&appCtx);

    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{});
    if (status != 0) { g_log << "Setup failed: 0x" << std::hex << status << std::endl; return 1; }

    status = runtime.LoadXexImage("game:\\default.xex");
    if (status != 0) { g_log << "XEX failed: 0x" << std::hex << status << std::endl; return 1; }

    auto thread = runtime.LaunchModule();
    g_log << "Launched OK" << std::endl;

    int result = appCtx.RunMainMessageLoop();
    g_log << "Exit: " << result << std::endl;
    CoUninitialize();
    return result;
}
