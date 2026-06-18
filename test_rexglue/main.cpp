// GTA V — rexglue Vulkan baseline + window
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/graphics/graphics_system.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context_win.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xobject.h>
#include <rex/system/util/object_table.h>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <chrono>
#include <set>
#include <mutex>

// === BATCH COLLECTOR: override ResolveIndirectFunction ===
// The recompiler misses some indirect-call targets (function entries reached
// only via vtable/function-pointer). The stock runtime FATALs on the first
// one. We override the resolver to instead log every missing address and
// return a no-op stub, so a single run surfaces ALL missing functions. They
// then get batch-added to gta5_manifest.toml [entrypoint.functions].
static void NoOpMissingStub(PPCContext& ctx, uint8_t* base) { (void)ctx; (void)base; }
namespace rex::runtime {
PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
    auto* rt = rex::Runtime::instance();
    if (rt && rt->function_dispatcher()) {
        PPCFunc* f = rt->function_dispatcher()->GetFunction(guest_address);
        if (f) return f;
    }
    static std::mutex mtx;
    static std::set<uint32_t> seen;
    std::lock_guard<std::mutex> lk(mtx);
    if (seen.insert(guest_address).second) {
        FILE* f = fopen("gta5_missing_funcs.txt", "a");
        if (f) { fprintf(f, "0x%08X\n", guest_address); fclose(f); }
    }
    return &NoOpMissingStub;
}
}  // namespace rex::runtime

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

// === DIAGNOSTIC: GPU interrupt callback probe ===
// 0x836B1768 is the GPU interrupt handler the game registered via
// SetInterruptCallback. If rexglue's vsync worker dispatches VBlank, this
// fires every frame. If it never fires, the dispatch path is broken.
REX_EXTERN(__imp__sub_836B1768);
static std::atomic<uint64_t> g_irqCount{0};
REX_HOOK_RAW(sub_836B1768) {
    uint64_t n = g_irqCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == 1 || n % 60 == 0) {
        FILE* f = fopen("gta5_irq.txt", "a");
        if (f) {
            fprintf(f, "GPU interrupt cb sub_836B1768 fired: count=%llu r3=0x%llX r4=0x%llX\n",
                (unsigned long long)n,
                (unsigned long long)ctx.r3.u64, (unsigned long long)ctx.r4.u64);
            fclose(f);
        }
    }
    __imp__sub_836B1768(ctx, base);
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

    // Watchdog: sample the main guest thread's context to locate the hang.
    // Recompiled code has no live PC, but lr (last call return addr), r1
    // (stack ptr) and last_indirect_target pinpoint a busy-wait region.
    rex::system::XThread* mainThread = thread.get();
    std::thread watchdog([mainThread]{
        bool dumped = false;
        for (int s = 1; ; ++s) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            PPCContext* c = mainThread ? mainThread->thread_state()->context() : nullptr;
            FILE* f = fopen("gta5_mainthread.txt", "a");
            if (f && c) {
                fprintf(f, "t=%2ds lr=0x%08llX r1=0x%08X ctr=0x%08X indir=0x%08X r3=0x%08X r11=0x%08X\n",
                    s, (unsigned long long)c->lr, c->r1.u32, c->ctr.u32,
                    c->last_indirect_target, c->r3.u32, c->r11.u32);
                fclose(f);
            }
            // One-time dump: what object is the main thread waiting on + all threads.
            if (!dumped && s >= 3) {
                dumped = true;
                auto* ks = rex::Runtime::instance() ? rex::Runtime::instance()->kernel_state() : nullptr;
                FILE* g = fopen("gta5_objdump.txt", "w");
                if (g && ks && c) {
                    uint32_t waitHandle = c->r3.u32;
                    auto obj = ks->object_table()->LookupObject<rex::system::XObject>(waitHandle);
                    fprintf(g, "main wait handle 0x%08X -> type=%u name='%s'\n",
                        waitHandle, obj ? (unsigned)obj->type() : 999u,
                        obj ? obj->name().c_str() : "(null)");
                    auto threads = ks->object_table()->GetObjectsByType<rex::system::XThread>(
                        rex::system::XObject::Type::Thread);
                    fprintf(g, "threads=%zu\n", threads.size());
                    for (auto& t : threads) {
                        if (!t) continue;
                        PPCContext* tc = t->thread_state() ? t->thread_state()->context() : nullptr;
                        fprintf(g, "  thread '%s' id=%X lr=0x%08llX r1=0x%08X r3=0x%08X\n",
                            t->name().c_str(), t->thread_id(),
                            tc ? (unsigned long long)tc->lr : 0ull,
                            tc ? tc->r1.u32 : 0u, tc ? tc->r3.u32 : 0u);
                    }
                    fclose(g);
                }
            }
        }
    });
    watchdog.detach();

    // 7. Windows message loop
    int result = app_context.RunMainMessageLoop();

    fprintf(stderr, "Done, result=%d\n", result);
    return result;
}
