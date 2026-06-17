// GTA V — direct rex::Runtime usage
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/system/xthread.h>
#include <cstdio>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#include <windows.h>

// Vectored exception handler to catch game crashes
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;
    void* addr = info->ExceptionRecord->ExceptionAddress;
    printf("\n!!! GAME CRASH !!!\n");
    printf("  Exception: 0x%08lX at %p\n", code, addr);
    if (info->ExceptionRecord->NumberParameters >= 2) {
        printf("  Info: type=%lld addr=0x%016llX\n",
               info->ExceptionRecord->ExceptionInformation[0],
               info->ExceptionRecord->ExceptionInformation[1]);
    }
    // Print register state
    auto* ctx = info->ContextRecord;
    printf("  RAX=0x%016llX RBX=0x%016llX RCX=0x%016llX\n",
           ctx->Rax, ctx->Rbx, ctx->Rcx);
    printf("  RDX=0x%016llX RSI=0x%016llX RDI=0x%016llX\n",
           ctx->Rdx, ctx->Rsi, ctx->Rdi);
    printf("  R8 =0x%016llX R9 =0x%016llX R10=0x%016llX\n",
           ctx->R8, ctx->R9, ctx->R10);
    printf("  RSP=0x%016llX RBP=0x%016llX RIP=0x%016llX\n",
           ctx->Rsp, ctx->Rbp, ctx->Rip);
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH; // Let it crash with info printed
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

    printf("Setting up runtime (tool_mode=true, skip GPU)...\n");
    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{.tool_mode = true});
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
