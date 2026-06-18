// GTA V — rexglue Vulkan baseline + window
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/graphics/graphics_system.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context_win.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <chrono>

// === DIAGNOSTIC: render-loop detection ===
// sub_822D41E8 is the GTA V render function that calls VdSwap. If this is
// called the game reached its render loop; if never, it is stuck in init
// (busy-wait). The hook writes proof to a flushed file directly from the
// guest thread (a detached std::thread proved unreliable here).
REX_EXTERN(__imp__sub_822D41E8);
static std::atomic<uint64_t> g_swapCount{0};
REX_HOOK_RAW(sub_822D41E8) {
    uint64_t n = g_swapCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == 1 || n % 60 == 0) {
        FILE* f = fopen("gta5_swap.txt", "a");
        if (f) {
            fprintf(f, "sub_822D41E8 (render/VdSwap) called: count=%llu\n",
                (unsigned long long)n);
            fclose(f);
        }
    }
    __imp__sub_822D41E8(ctx, base);
}

// === DIAGNOSTIC: entry-point probe ===
// xstart is the XEX entry (0x83639888). Proves the hook-override mechanism
// works and shows whether the entry runs and ever returns.
REX_EXTERN(__imp__xstart);
REX_HOOK_RAW(xstart) {
    FILE* f = fopen("gta5_xstart.txt", "a");
    if (f) { fprintf(f, "xstart ENTERED\n"); fclose(f); }
    __imp__xstart(ctx, base);
    f = fopen("gta5_xstart.txt", "a");
    if (f) { fprintf(f, "xstart RETURNED\n"); fclose(f); }
}

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
static LONG WINAPI PageFaultHandler(EXCEPTION_POINTERS* i) {
    if (i->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t a = i->ExceptionRecord->ExceptionInformation[1];
        if (a > 0x10000) { // Don't commit near-NULL pages
            VirtualAlloc((LPVOID)(a & ~0xFFFULL), 0x1000, MEM_COMMIT, PAGE_READWRITE);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* i) {
    uintptr_t code = i->ExceptionRecord->ExceptionCode;
    uintptr_t addr = i->ExceptionRecord->ExceptionAddress ? (uintptr_t)i->ExceptionRecord->ExceptionAddress : 0;
    uintptr_t mem = (code == 0xC0000005) ? i->ExceptionRecord->ExceptionInformation[1] : 0;
    fprintf(stderr, "\n!!! CRASH: code=0x%lX addr=0x%lX mem=0x%lX\n",
        (unsigned long)code, (unsigned long)addr, (unsigned long)mem);
    FILE* f = fopen("gta5_rexglue_crash.txt", "w");
    if (f) { fprintf(f, "code=0x%lX addr=0x%lX mem=0x%lX\n",
        (unsigned long)code, (unsigned long)addr, (unsigned long)mem); fclose(f); }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main() {
    std::ofstream log("gta5_rexglue.log");
    fprintf(stderr, "=== GTA V rexglue Vulkan ===\n");

    // Enable rexglue's own kernel/GPU logging to a file (debug level, flush
    // every message) so we can see which kernel functions the game calls
    // during init and where it stops.
    rex::LogConfig logcfg;
    logcfg.default_level = spdlog::level::debug;
    logcfg.log_to_console = false;
    logcfg.log_file = "gta5_kernel.log";
    logcfg.flush_level = spdlog::level::trace;
    rex::InitLogging(logcfg);

#ifdef _WIN32
    AddVectoredExceptionHandler(1, PageFaultHandler);
    SetUnhandledExceptionFilter(CrashHandler);
#endif

    // 1. Create Runtime
    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";
    rex::Runtime runtime(gameRoot, userRoot);
    fprintf(stderr, "[1/5] Runtime created\n");

    // 2. Create Win32 app context BEFORE Setup (so SetupPresentation works)
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    rex::ui::Win32WindowedAppContext app_context(hInstance, SW_SHOWNORMAL);
    if (!app_context.Initialize()) {
        log << "AppContext failed" << std::endl;
        fprintf(stderr, "FAIL: AppContext init\n");
        return 1;
    }
    runtime.set_app_context(&app_context);
    fprintf(stderr, "[2/5] AppContext OK\n");

    // 3. Setup WITH app_context set (enables full presentation path)
    rex::RuntimeConfig cfg;
    cfg.graphics = REX_GRAPHICS_BACKEND(rex::graphics::vulkan::VulkanGraphicsSystem);
    auto status = runtime.Setup(PPCImageConfig, std::move(cfg));
    if (status != 0) {
        log << "Setup failed: 0x" << std::hex << status << std::endl;
        fprintf(stderr, "FAIL: Setup returned 0x%X\n", (unsigned)status);
        return 1;
    }
    log << "Setup OK" << std::endl;
    auto* gs = static_cast<rex::graphics::GraphicsSystem*>(runtime.graphics_system());
    fprintf(stderr, "[3/5] Setup OK, graphics=%s, presenter=%s\n",
        gs ? "YES" : "NO",
        (gs && gs->presenter()) ? "YES" : "NO");

    // 4. Create window and connect presenter
    auto window = rex::ui::Window::Create(app_context, "GTA V (rexglue Vulkan)", 1280, 720);
    if (!window) {
        log << "Window failed" << std::endl;
        fprintf(stderr, "FAIL: Window creation\n");
        return 1;
    }
    window->Open();
    if (gs && gs->presenter()) {
        window->SetPresenter(gs->presenter());
    }
    fprintf(stderr, "[4/5] Window opened\n");

    // 5. Load XEX and launch game
    status = runtime.LoadXexImage("game:\\default.xex");
    if (status != 0) {
        log << "XEX failed" << std::endl;
        fprintf(stderr, "FAIL: XEX load\n");
        return 1;
    }
    auto thread = runtime.LaunchModule();
    log << "Launched" << std::endl;
    log.flush();
    fprintf(stderr, "[5/5] XEX loaded + Launched\nEntering message loop...\n");

    // 7. Windows message loop
    int result = app_context.RunMainMessageLoop();

    fprintf(stderr, "Done, result=%d\n", result);
    return result;
}
