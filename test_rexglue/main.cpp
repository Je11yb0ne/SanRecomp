// GTA V — direct rex::Runtime usage
#include "generated/default/gta5_recomp_init.h"
#include <rex/runtime.h>
#include <rex/system/xthread.h>
#include <cstdio>
#include <filesystem>
#include <thread>

int main() {
    printf("=== GTA V rexglue Runtime ===\n");

    std::filesystem::path gameRoot = "D:/Games/Xenia/gta5";
    std::filesystem::path userRoot = "C:/Users/Jellybone/AppData/Roaming/SanRecomp/save";

    rex::Runtime runtime(gameRoot, userRoot);

    printf("Setting up runtime...\n");
    auto status = runtime.Setup(PPCImageConfig, rex::RuntimeConfig{});
    if (status != 0) {
        printf("Runtime setup failed: 0x%08X\n", (unsigned)status);
        return 1;
    }
    printf("Runtime setup OK\n");

    printf("Loading XEX from game_data_root=%s\n", gameRoot.string().c_str());
    // VFS mounts game_data_root at d: and game: symbolic links
    status = runtime.LoadXexImage("game:\\default.xex");
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
