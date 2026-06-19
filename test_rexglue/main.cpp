// GTA V — rexglue Vulkan baseline + window
#include "generated/disc2/gta5_recomp_init.h"
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
#include <rex/kernel/init.h>
#include <rex/audio/sdl/sdl_audio_system.h>
#include <rex/input/input_system.h>
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
static std::atomic<uint8_t*> g_guest_base{nullptr};  // captured for VEH dumps
REX_HOOK_RAW(xstart) {
    g_guest_base.store(base, std::memory_order_relaxed);
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
    g_guest_base.store(base, std::memory_order_relaxed);
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

// === DIAGNOSTIC: semaphore wait/release tracing ===
// sub_823C7A68 = game wait wrapper (r3=handle), sub_8239CBA8 = game release
// wrapper (r3=handle). Logging both shows whether semaphore 0xF80008F0 (the
// main thread's wait) is ever released, and the full wait/release pattern.
static void diag_sync_log(const char* op, uint32_t handle) {
    uint32_t tid = 0;
    auto* t = rex::system::XThread::GetCurrentThread();
    if (t) tid = t->thread_id();
    FILE* f = fopen("gta5_sync.txt", "a");
    if (f) { fprintf(f, "[tid=%X] %s 0x%08X\n", tid, op, handle); fclose(f); }
}
REX_EXTERN(__imp__sub_823C7A68);
REX_HOOK_RAW(sub_823C7A68) {
    diag_sync_log("WAIT   ", ctx.r3.u32);
    __imp__sub_823C7A68(ctx, base);
}
REX_EXTERN(__imp__sub_8239CBA8);
REX_HOOK_RAW(sub_8239CBA8) {
    diag_sync_log("RELEASE", ctx.r3.u32);
    __imp__sub_8239CBA8(ctx, base);
}

// === DISC STATE MACHINE BYPASS (necessary until content setup is perfect) ===
// The game's internal state machine requires proper installation flow which we
// can't replicate with pre-populated content alone.  These hooks let the game
// reach the loading screen; the remaining blocker is the loading deadlock.
REX_HOOK_RAW(sub_82985760) { (void)base; ctx.r3.u64 = 0; }  // master switch
REX_HOOK_RAW(sub_8299FBF8) {
    if ((ctx.r4.u32 & 0xFF) == 1) { ctx.r3.u64 = 0; return; }
    __imp__sub_8299FBF8(ctx, base);
}
REX_HOOK_RAW(sub_8299BD70) {
    if ((ctx.r3.u32 & 0xFF) == 2) { ctx.r3.u64 = 1; return; }
    __imp__sub_8299BD70(ctx, base);
    ctx.r3.u64 = 1;
}
REX_HOOK_RAW(sub_8364D6C8) { __imp__XamSwapDisc(ctx, base); }

// === DIAGNOSTIC: XamContentCreateEx content_data probe ===
// sub_8363A3B8 is the guest XamContentCreateEx wrapper; it throws
// utf8::invalid_utf8 in the kernel when content_data.file_name_raw holds
// invalid UTF-8. It is called INDIRECTLY (weak-alias hook misses it), so we
// install via FunctionDispatcher::SetFunction in main(). content_data layout:
// content_type@4, file_name_raw@0x108 (42 bytes).
static PPCFunc* g_orig_content_open = nullptr;
static void ProbeContentOpen(PPCContext& ctx, uint8_t* base) {
    FILE* f = fopen("gta5_content_open.txt", "a");
    if (f) {
        uint32_t cd = ctx.r5.u32, rn = ctx.r4.u32, flags = ctx.r6.u32;
        uint32_t devid = 0, ctype = 0;
        char fname[48] = {0}, root[16] = {0};
        if (cd) {
            const uint8_t* p = base + cd;
            devid = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
            ctype = (p[4]<<24)|(p[5]<<16)|(p[6]<<8)|p[7];
            for (int k = 0; k < 42; ++k) fname[k] = (char)p[0x108 + k];
        }
        if (rn) { for (int k = 0; k < 15; ++k) { char c = (char)base[rn + k]; root[k]=c; if(!c) break; } }
        fprintf(f, "XamContentCreateEx user=0x%X root='%s' cd=0x%08X flags=0x%X devid=0x%X ctype=0x%X fn_hex=",
            ctx.r3.u32, root, cd, flags, devid, ctype);
        for (int k = 0; k < 42; ++k) fprintf(f, "%02X", (unsigned char)fname[k]);
        fprintf(f, " fn_ascii='");
        for (int k = 0; k < 42 && fname[k]; ++k) fprintf(f, "%c", (fname[k]>=32&&fname[k]<127)?fname[k]:'.');
        fprintf(f, "'\n");
        fclose(f);
    }
    if (g_orig_content_open) g_orig_content_open(ctx, base);
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
    // First-chance capture of the C++ throw (0xE06D7363) — the stack is intact
    // here, so the guest thread's lr/ctr/last_indirect_target are FRESH (the
    // unhandled filter sees stale values), and a host backtrace pinpoints the
    // throwing DLL function. Log once, then let it propagate.
    if (i->ExceptionRecord->ExceptionCode == 0xE06D7363) {
        static std::atomic<bool> once{false};
        bool exp = false;
        if (once.compare_exchange_strong(exp, true)) {
            FILE* f = fopen("gta5_throw_trace.txt", "w");
            if (f) {
                auto* t = rex::system::XThread::GetCurrentThread();
                if (t && t->thread_state() && t->thread_state()->context()) {
                    auto* c = t->thread_state()->context();
                    fprintf(f, "guest tid=%X lr=0x%08llX ctr=0x%08X indir=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X r11=0x%08X\n",
                        t->thread_id(), (unsigned long long)c->lr, c->ctr.u32,
                        c->last_indirect_target, c->r3.u32, c->r4.u32, c->r5.u32, c->r6.u32, c->r11.u32);
                    // r5 = content_data guest ptr; dump content_type@4 + file_name_raw@0x108 (42).
                    uint8_t* gb = g_guest_base.load(std::memory_order_relaxed);
                    if (gb && c->r5.u32) {
                        const uint8_t* p = gb + c->r5.u32;
                        uint32_t ctype = (p[4]<<24)|(p[5]<<16)|(p[6]<<8)|p[7];
                        fprintf(f, "content_data: ctype=0x%X file_name_hex=", ctype);
                        for (int k = 0; k < 42; ++k) fprintf(f, "%02X", p[0x108 + k]);
                        fprintf(f, " ascii='");
                        for (int k = 0; k < 42; ++k){ uint8_t b=p[0x108+k]; if(!b)break; fprintf(f, "%c", (b>=32&&b<127)?b:'.'); }
                        fprintf(f, "'\n display_name_hex=");
                        for (int k = 0; k < 32; ++k) fprintf(f, "%02X", p[8 + k]);
                        fprintf(f, "\n");
                    }
                } else {
                    fprintf(f, "(no guest thread context)\n");
                }
                HMODULE dll = GetModuleHandleA("rexruntimerd.dll");
                HMODULE exe = GetModuleHandleA(nullptr);
                fprintf(f, "rexruntimerd.dll base=%p  exe base=%p\n", (void*)dll, (void*)exe);
                void* bt[32];
                USHORT n = CaptureStackBackTrace(0, 32, bt, nullptr);
                for (USHORT k = 0; k < n; ++k) {
                    uintptr_t a = (uintptr_t)bt[k];
                    const char* mod = "?"; uintptr_t off = a;
                    if (dll && a >= (uintptr_t)dll) { mod = "dll"; off = a - (uintptr_t)dll; }
                    else if (exe && a >= (uintptr_t)exe) { mod = "exe"; off = a - (uintptr_t)exe; }
                    fprintf(f, "  [%2d] %s+0x%llX (abs=%p)\n", k, mod, (unsigned long long)off, bt[k]);
                }
                fclose(f);
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* i) {
    uintptr_t code = i->ExceptionRecord->ExceptionCode;
    uintptr_t addr = i->ExceptionRecord->ExceptionAddress ? (uintptr_t)i->ExceptionRecord->ExceptionAddress : 0;
    uintptr_t mem = (code == 0xC0000005) ? i->ExceptionRecord->ExceptionInformation[1] : 0;
    // Capture the crashing guest thread's PPC context to locate the guest fn.
    uint64_t glr = 0; uint32_t gr1 = 0, gr3 = 0, gtid = 0;
    auto* t = rex::system::XThread::GetCurrentThread();
    if (t && t->thread_state() && t->thread_state()->context()) {
        auto* c = t->thread_state()->context();
        glr = c->lr; gr1 = c->r1.u32; gr3 = c->r3.u32; gtid = t->thread_id();
    }
    // Decode MSVC C++ exceptions (0xE06D7363) to get the thrown type name +
    // (for std::exception subclasses) the what() message. This pinpoints which
    // rexglue/std throw fired.
    char cxxinfo[512] = {0};
    if (code == 0xE06D7363 && i->ExceptionRecord->NumberParameters >= 3) {
        auto* rec = i->ExceptionRecord;
        uintptr_t base = (rec->NumberParameters >= 4) ? rec->ExceptionInformation[3] : 0;
        const uint8_t* obj = (const uint8_t*)rec->ExceptionInformation[1];
        const uint8_t* throwInfo = (const uint8_t*)rec->ExceptionInformation[2];
        __try {
            if (throwInfo && base) {
                int32_t cta_rva = *(const int32_t*)(throwInfo + 12);   // pCatchableTypeArray
                const uint8_t* cta = (const uint8_t*)(base + cta_rva);
                int n = *(const int*)cta;
                if (n > 0) {
                    int32_t ct_rva = *(const int32_t*)(cta + 4);       // first CatchableType
                    const uint8_t* ct = (const uint8_t*)(base + ct_rva);
                    int32_t td_rva = *(const int32_t*)(ct + 4);        // pType -> TypeDescriptor
                    const char* name = (const char*)(base + td_rva + 16); // TypeDescriptor.name
                    // Try what(): MSVC std::exception stores char* _Data._What at +8
                    const char* what = nullptr;
                    if (obj) { const char* w = *(const char* const*)(obj + 8); if (w) what = w; }
                    _snprintf_s(cxxinfo, sizeof(cxxinfo), _TRUNCATE,
                        " | C++ throw type='%s' what='%s'", name ? name : "?", what ? what : "");
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            _snprintf_s(cxxinfo, sizeof(cxxinfo), _TRUNCATE, " | C++ throw (decode failed)");
        }
    }
    fprintf(stderr, "\n!!! CRASH: code=0x%lX addr=0x%lX mem=0x%lX | guest tid=%X lr=0x%llX r1=0x%X r3=0x%X%s\n",
        (unsigned long)code, (unsigned long)addr, (unsigned long)mem,
        gtid, (unsigned long long)glr, gr1, gr3, cxxinfo);
    FILE* f = fopen("gta5_rexglue_crash.txt", "w");
    if (f) { fprintf(f, "code=0x%lX addr=0x%lX mem=0x%lX\nguest tid=%X lr=0x%llX r1=0x%X r3=0x%X%s\n",
        (unsigned long)code, (unsigned long)addr, (unsigned long)mem,
        gtid, (unsigned long long)glr, gr1, gr3, cxxinfo); fclose(f); }
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
    // Trace kernel category to capture semaphore/event/wait ops (Nt*/Ke*).
    logcfg.category_levels["krnl"] = spdlog::level::trace;
    rex::InitLogging(logcfg);

#ifdef _WIN32
    AddVectoredExceptionHandler(1, PageFaultHandler);
    SetUnhandledExceptionFilter(CrashHandler);
#endif

    // 0. Disc selection (reblue-style: let user pick disc paths before boot)
    // Use known defaults if already configured.
    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";
    fprintf(stderr, "[0/6] game_root=%s\n", gameRoot.string().c_str());

    // 1. Create Runtime
    rex::Runtime runtime(gameRoot, userRoot);
    fprintf(stderr, "[1/6] Runtime created\n");

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

    // 3. Setup WITH app_context set (enables full presentation path).
    // CRITICAL: provide audio/input/kernel_init like ReXApp does — without
    // these the game's audio/input/kernel-dependent threads deadlock waiting
    // for subsystems that were never initialized. (skate3recomp relies on
    // these ReXApp defaults; our manual setup was missing them.)
    rex::RuntimeConfig cfg;
    cfg.graphics = REX_GRAPHICS_BACKEND(rex::graphics::vulkan::VulkanGraphicsSystem);
    cfg.audio_factory = REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem);
    cfg.input_factory = REX_INPUT_BACKEND(rex::input::CreateDefaultInputSystem);
    cfg.kernel_init = rex::kernel::InitializeKernel;
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
    fprintf(stderr, "[diag] input_system=%p audio_system=%p\n",
        (void*)runtime.input_system(), (void*)runtime.audio_system());

    // Install indirect-call probe on guest XamContentCreateEx wrapper.
    if (auto* fd = runtime.function_dispatcher()) {
        g_orig_content_open = fd->GetFunction(0x8363A3B8);
        fd->SetFunction(0x8363A3B8, &ProbeContentOpen);
        fprintf(stderr, "[diag] content-open probe installed (orig=%p)\n", (void*)g_orig_content_open);
    }

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
    // Attach the window to the input system — input drivers are created with a
    // null window and need this to avoid a null-deref in XamInputGetState.
    // (ReXApp does this in SetupPresentation; our manual setup was missing it.)
    if (auto* is = static_cast<rex::input::InputSystem*>(runtime.input_system())) {
        is->AttachWindow(window.get());
        fprintf(stderr, "[4/5] Window opened + input attached\n");
    } else {
        fprintf(stderr, "[4/5] Window opened (no input system!)\n");
    }

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
