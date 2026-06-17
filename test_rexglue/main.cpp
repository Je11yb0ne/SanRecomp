// GTA V — WIN32 app with file-based logging
#include "generated/default/gta5_recomp_init.h"
#include <rex/hook.h>

// Hook VdSwap to check if the game's render loop is running
// Must use printf (not g_log) because these run at global scope before main()
#define VDSWAP_LOG(fmt, ...) do { \
    FILE* f = fopen("gta5_rexglue.log", "a"); \
    if(f) { fprintf(f, fmt "\n", ##__VA_ARGS__); fclose(f); } \
} while(0)
// No GPU hooks — let rexglue kernel handle all GPU functions internally
// The kernel has MMIO interception, PM4 command processing, and Vulkan/D3D12 backends
// Hooking VdSwap/VdGetSystemCommandBuffer prevents rexglue from working properly
#include <rex/runtime.h>
#include <rex/graphics/vulkan/graphics_system.h>
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

int main() {
    g_log.open("gta5_rexglue.log");
    g_log << "=== GTA V rexglue WIN32 ===" << std::endl;

#ifdef _WIN32
    AddVectoredExceptionHandler(1, PageFaultHandler);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";
    rex::Runtime runtime(gameRoot, userRoot);

    // Explicitly use Vulkan backend
    // Default config — rexglue auto-creates graphics backend
    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{});
    if (status != 0) { g_log << "Setup failed: 0x" << std::hex << status << std::endl; return 1; }

    status = runtime.LoadXexImage("game:\\default.xex");
    if (status != 0) { g_log << "XEX failed: 0x" << std::hex << status << std::endl; return 1; }

    auto thread = runtime.LaunchModule();
    g_log << "Launched OK" << std::endl;

    uint8_t* membase = runtime.virtual_membase();

    // Start VBlank timer — game needs periodic VBlank interrupts to begin rendering
    std::atomic<bool> vblankStop{false};
    std::thread vblankThread([&]() {
        uint32_t tick = 0;
        VDSWAP_LOG("[VBlank] thread starting");
        while (!vblankStop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            tick++;
            if (tick <= 3 || tick % 60 == 0) {
                VDSWAP_LOG("[VBlank] alive tick %d", tick);
            }
            // Don't call VdSwap directly — it crashes. Game's render callback
            // should be triggered by rexglue's internal VBlank interrupt.
        }
    });
    g_log << "VBlank timer started" << std::endl;
    g_log.flush();

    // Commit all likely rendering targets
    uint32_t scanAddrs[] = {0xE0000000, 0xE0100000, 0xE0500000, 0xE0800000,
                            0x83000000, 0x84000000, 0x85000000, 0x90000000};
    for (uint32_t off : scanAddrs) {
        VirtualAlloc(membase + off, 0x500000, MEM_COMMIT, PAGE_READWRITE);
    }
    g_log << "Memory regions committed" << std::endl;

    // GPU MMIO scan: detect what registers the game writes
    uint8_t* gpuMmio = membase + 0x7FC80000;
    uint32_t gpuSize = 0x80000; // 512KB
    VirtualAlloc(gpuMmio, gpuSize, MEM_COMMIT, PAGE_READWRITE);
    std::vector<uint32_t> prevState(gpuSize / 4, 0);
    for (int tick = 0; tick < 20; tick++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bool changed = false;
        uint32_t* cur = (uint32_t*)gpuMmio;
        for (uint32_t i = 0; i < gpuSize / 4; i++) {
            uint32_t val = cur[i]; // raw (big-endian in memory)
            if (val != prevState[i]) {
                if (!changed) {
                    g_log << "[GPU-MMIO] Tick " << tick << " changes:" << std::endl;
                    changed = true;
                }
                g_log << "  reg=0x" << std::hex << i << " prev=0x" << prevState[i]
                      << " cur=0x" << val << std::dec << std::endl;
                prevState[i] = val;
            }
        }
        if (changed) g_log.flush();
    }
    g_log << "GPU MMIO scan complete" << std::endl;
    g_log.flush();

    // Scan all memory regions for non-zero pixels (game render output)
    for (int tick = 0; tick < 30; tick++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        bool found = false;
        for (uint32_t off : scanAddrs) {
            uint8_t* p = membase + off;
            int nz = 0;
            for (int i = 0; i < 1280*720 && i*4 < 0x500000; i++)
                if (p[i*4] || p[i*4+1] || p[i*4+2] || p[i*4+3]) nz++;
            if (nz > 0) {
                g_log << "[RENDER] Tick" << tick << " addr=0x" << std::hex << off
                      << " pixels=" << std::dec << nz << std::endl;
                found = true; break;
            }
        }
        if (found) { g_log.flush(); break; }
    }
    g_log << "Render scan complete" << std::endl;
    g_log.flush();

    // Keep main thread alive while game runs (no external window context)
    g_log << "Main thread waiting..." << std::endl;
    g_log.flush();
    while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    CoUninitialize();
    return 0;
}
