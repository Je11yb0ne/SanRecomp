#pragma once
// Resource declarations for bc_diff and other embedded assets
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
extern const uint8_t g_button_bc_diff[1];
extern const size_t g_button_bc_diff_size;
extern const size_t g_button_bc_diff_uncompressed_size;
// Placeholder stubs for missing resources
extern const uint8_t g_trophy[1];
extern const size_t g_trophy_uncompressed_size;
extern const uint8_t g_im_font_atlas[1];
extern const size_t g_im_font_atlas_uncompressed_size;
// Font atlas texture (used by video.cpp)
extern const uint8_t g_im_font_atlas_texture[1];
extern const size_t g_im_font_atlas_texture_size;
extern const size_t g_im_font_atlas_texture_uncompressed_size;
// UI image resources (imgui_utils.cpp, achievement_menu.cpp, installer_wizard.cpp)
extern const uint8_t g_button_window[1];
extern const size_t g_button_window_uncompressed_size;
extern const uint8_t g_controller[1];
extern const size_t g_controller_uncompressed_size;
extern const uint8_t g_kbm[1];
extern const size_t g_kbm_uncompressed_size;
extern const uint8_t g_window[1];
extern const size_t g_window_uncompressed_size;
extern const uint8_t g_select_arrow[1];
extern const size_t g_select_arrow_uncompressed_size;
extern const uint8_t g_main_menu1[1];
extern const size_t g_main_menu1_uncompressed_size;
extern const uint8_t g_arrow[1];
extern const size_t g_arrow_uncompressed_size;
// Installer wizard images
extern const uint8_t g_install_001[1];
extern const size_t g_install_001_uncompressed_size;
extern const uint8_t g_install_002[1];
extern const size_t g_install_002_uncompressed_size;
extern const uint8_t g_install_003[1];
extern const size_t g_install_003_uncompressed_size;
extern const uint8_t g_install_004[1];
extern const size_t g_install_004_uncompressed_size;
extern const uint8_t g_install_005[1];
extern const size_t g_install_005_uncompressed_size;
extern const uint8_t g_install_006[1];
extern const size_t g_install_006_uncompressed_size;
extern const uint8_t g_install_007[1];
extern const size_t g_install_007_uncompressed_size;
extern const uint8_t g_install_008[1];
extern const size_t g_install_008_uncompressed_size;
extern const uint8_t g_libertyrecomp[1];
extern const size_t g_libertyrecomp_uncompressed_size;
extern const uint8_t g_gta5_logo[1];
extern const size_t g_gta5_logo_uncompressed_size;
extern const uint8_t g_tlad_logo[1];
extern const size_t g_tlad_logo_uncompressed_size;
extern const uint8_t g_tbogt_logo[1];
extern const size_t g_tbogt_logo_uncompressed_size;
// SPIR-V shader cache (used by video.cpp)
extern const uint8_t g_spirvCache[1];
extern const size_t g_spirvCacheCompressedSize;
extern const size_t g_spirvCacheDecompressedSize;
// DXIL shader cache
extern const uint8_t g_dxilCache[1];
extern const size_t g_dxilCacheCompressedSize;
extern const size_t g_dxilCacheDecompressedSize;
// AIR shader cache (Metal)
extern const uint8_t g_airCache[1];
extern const size_t g_airCacheCompressedSize;
extern const size_t g_airCacheDecompressedSize;
#ifdef __cplusplus
}
#endif

// Block compression diff/patch structures (used by video.cpp for texture patching)
#ifdef __cplusplus
#include <cstdint>

struct BlockCompressionDiffPatchHeader {
    uint32_t entriesOffset;
    uint32_t entryCount;
};

struct BlockCompressionDiffPatchEntry {
    uint64_t hash;
    uint32_t patchesOffset;
    uint32_t patchCount;
};

struct BlockCompressionDiffPatch {
    uint32_t destinationOffset;
    uint32_t patchBytesOffset;
    uint32_t patchBytesSize;
};
#endif
