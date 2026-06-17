// GTA V — direct rex::Runtime usage
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/system/xthread.h>
#include <cstdio>
#include <filesystem>
#include <thread>

int main() {
    printf("=== GTA V rexglue Runtime ===\n");

    std::filesystem::path gameRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/game";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";

    rex::Runtime runtime(gameRoot, userRoot);

    printf("Setting up runtime...\n");
    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{});
    if (status != 0) {
        printf("Runtime setup failed: 0x%08X\n", (unsigned)status);
        return 1;
    }
    printf("Runtime setup OK\n");

    printf("Loading XEX...\n");
    status = runtime.LoadXexImage("default.xex");
    if (status != 0) {
        printf("LoadXexImage failed: 0x%08X\n", (unsigned)status);
        return 1;
    }
    printf("XEX loaded OK\n");

    printf("Launching game...\n");
    auto thread = runtime.LaunchModule();
    printf("Game thread launched\n");

    printf("Entering event loop...\n");
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
