// GTA V — direct rex::Runtime usage
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/system/xthread.h>
#include <cstdio>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#include <windows.h>

// Vectored exception handler — catches access violations and maps missing pages
// GTA V accesses Xbox 360 physical addresses (0x00000000-0x7FFFFFFF) which aren't
// in the normal guest memory range (0x80000000+). We dynamically commit these.
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    if (code == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t faultAddr = info->ExceptionRecord->ExceptionInformation[1];
        ULONG_PTR page = faultAddr & ~0xFFFULL;

        // Try to commit the page (handles physical address accesses)
        LPVOID result = VirtualAlloc((LPVOID)page, 0x1000, MEM_COMMIT, PAGE_READWRITE);
        if (result) {
            static int s_commitCount = 0;
            if (++s_commitCount <= 20) {
                printf("[MEM] Committed page at %p (fault=0x%llX)\n", (void*)page, faultAddr);
                fflush(stdout);
            }
            return EXCEPTION_CONTINUE_EXECUTION; // Retry
        }

        // Couldn't commit — print crash info and let it die
        printf("\n!!! GAME CRASH (unfixable) !!!\n");
        printf("  Exception: 0x%08lX at %p\n", code, info->ExceptionRecord->ExceptionAddress);
        printf("  Fault addr: 0x%016llX\n", faultAddr);
        auto* ctx = info->ContextRecord;
        printf("  RIP=0x%016llX RSP=0x%016llX\n", ctx->Rip, ctx->Rsp);
        fflush(stdout);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main() {
#ifdef _WIN32
    AddVectoredExceptionHandler(1, CrashHandler);
#endif

    printf("=== GTA V rexglue Runtime ===\n");

    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";

    rex::Runtime runtime(gameRoot, userRoot);

    printf("Setting up runtime (GPU enabled)...\n");
    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{});
    if (status != 0) {
        printf("Runtime setup failed: 0x%08X\n", (unsigned)status);
        return 1;
    }
    printf("Runtime setup OK\n");

    printf("Loading XEX from game_data_root=%s\n", gameRoot.string().c_str());
    status = runtime.LoadXexImage("game:\\default.xex");
    if (status != 0) {
        printf("LoadXexImage failed: 0x%08X\n", (unsigned)status);
        return 1;
    }
    printf("XEX loaded OK\n");

    printf("Launching game...\n");
    auto thread = runtime.LaunchModule();
    printf("Game thread launched, waiting...\n");

    // Event loop
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
