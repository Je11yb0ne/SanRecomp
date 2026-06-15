// Link stubs for external libraries and disabled source files.
// TODO: Replace each section with proper library builds or source fixes.

#include <stdafx.h>
#include <SDL.h>
#include <memory>
#include <kernel/io/session_tracker.h>
#include <gpu/video.h>
#include <ui/game_window.h>
#include <ui/installer_wizard.h>
#include <hid/hid.h>
#include <patches/player_limit_patches.h>
#include <gpu/imgui/imgui_snapshot.h>

// SDL2 stubs removed — SDL2::SDL2-static now provides real implementations.

// =============================================================================
// ZSTD — decompression library
// =============================================================================
extern "C" {
size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t compressedSize) {
    (void)dst; (void)dstCapacity; (void)src; (void)compressedSize; return 0;
}
}

// Plume render interface — now built from thirdparty/plume

// =============================================================================
// ImGui SDL2 backend — stubs until implot/imgui SDL2 backend is compiled
// =============================================================================
bool ImGui_ImplSDL2_InitForOther(SDL_Window*) { return true; }
void ImGui_ImplSDL2_NewFrame(void) {}
bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event*) { return false; }

// =============================================================================
// ImFontAtlasSnapshot (from imgui_snapshot.cpp, disabled)
// =============================================================================
struct ImFontAtlas;
ImFontAtlas* ImFontAtlasSnapshot::Load(void) { return nullptr; }

// =============================================================================
// Achievement menu stubs
// =============================================================================
#include <ui/achievement_menu.h>
void AchievementMenu::Init(void) {}
void AchievementMenu::Draw(void) {}

// =============================================================================
// Installer wizard stubs (extra methods beyond Run)
// =============================================================================
void InstallerWizard::Init(void) {}
void InstallerWizard::Draw(void) {}

// =============================================================================
// ImGui utils
// =============================================================================
void InitImGuiUtils(void) {}

// =============================================================================
// Data definitions for resource variables (declared in headers but need definitions)
// =============================================================================
extern "C" {
extern const size_t g_button_bc_diff_uncompressed_size;
const size_t g_button_bc_diff_uncompressed_size = 1;
extern const size_t g_im_font_atlas_texture_uncompressed_size;
const size_t g_im_font_atlas_texture_uncompressed_size = 1;
extern const size_t g_spirvCacheCompressedSize;
const size_t g_spirvCacheCompressedSize = 1;
extern const size_t g_dxilCacheCompressedSize;
const size_t g_dxilCacheCompressedSize = 1;
extern const size_t g_spirvCacheDecompressedSize;
const size_t g_spirvCacheDecompressedSize = 1;
extern const size_t g_dxilCacheDecompressedSize;
const size_t g_dxilCacheDecompressedSize = 1;
}
// shader_cache.h C++-linkage symbols
extern const uint8_t g_compressedSpirvCache[];
const uint8_t g_compressedSpirvCache[1] = {};
extern const uint8_t g_compressedDxilCache[];
const uint8_t g_compressedDxilCache[1] = {};

// Additional zstd symbols
extern "C" {
unsigned ZSTD_isError(size_t code) { (void)code; return 0; }
const char* ZSTD_getErrorName(size_t code) { (void)code; return "unknown"; }
}

// DxcCreateInstance — resolved by linking dxcompiler.lib

// =============================================================================
// Shader cache symbols
// =============================================================================
#include <shader/shader_cache.h>
const size_t g_shaderCacheEntryCount = 0;
ShaderCacheEntry g_shaderCacheEntries[1] = {};

// =============================================================================
// GameNetworkingSockets
// =============================================================================
extern "C" {
int GameNetworkingSockets_Init(void*, char*) { return 1; }
void GameNetworkingSockets_Kill(void) {}
void* SteamNetworkingSockets_LibV12 = nullptr;
void* SteamNetworkingUtils_LibV4 = nullptr;
}

// =============================================================================
// FFmpeg (avcodec) — xma_decoder.cpp
// =============================================================================
extern "C" {
struct AVCodecContext;
struct AVCodec;
struct AVPacket;
struct AVFrame;
void* avcodec_find_decoder(int) { return nullptr; }
void* avcodec_alloc_context3(void*) { return nullptr; }
int avcodec_open2(void*, void*, void*) { return -1; }
int avcodec_send_packet(void*, void*) { return -1; }
int avcodec_receive_frame(void*, void*) { return -1; }
void* av_frame_alloc(void) { return nullptr; }
void* av_packet_alloc(void) { return nullptr; }
}

// =============================================================================
// SDL_mixer — embedded_player.cpp
// Note: actual signatures from SDL_mixer.h, stubs only for link
// =============================================================================
#include <SDL_mixer.h>
extern "C" {
int Mix_OpenAudio(int freq, Uint16 format, int channels, int chunksize) {
    (void)freq; (void)format; (void)channels; (void)chunksize; return 0;
}
Mix_Music* Mix_LoadMUS_RW(SDL_RWops* rw, int freesrc) { (void)rw; (void)freesrc; return nullptr; }
Mix_Chunk* Mix_LoadWAV_RW(SDL_RWops* rw, int freesrc) { (void)rw; (void)freesrc; return nullptr; }
int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops) { (void)channel; (void)chunk; (void)loops; return -1; }
int Mix_VolumeChunk(Mix_Chunk* chunk, int volume) { (void)chunk; (void)volume; return 0; }
void Mix_CloseAudio(void) {}
void Mix_FreeChunk(Mix_Chunk* chunk) { (void)chunk; }
void Mix_FreeMusic(Mix_Music* music) { (void)music; }
int Mix_PlayMusic(Mix_Music* music, int loops) { (void)music; (void)loops; return 0; }
int Mix_VolumeMusic(int volume) { (void)volume; return 0; }
int Mix_PlayingMusic(void) { return 0; }
int Mix_HaltMusic(void) { return 0; }
int Mix_FadeOutMusic(int ms) { (void)ms; return 1; }
void Mix_Quit(void) {}
}

// =============================================================================
// Networking helpers
// =============================================================================
namespace Net {
    const char* GameModeToString(GameMode) { return "unknown"; }
    const char* MapAreaToString(MapArea) { return "unknown"; }
    std::unique_ptr<ISessionTracker> CreateSessionTracker() { return nullptr; }
}

// =============================================================================
// Disabled source file symbols
// =============================================================================
// Video::CreateHostDevice and Video::WaitOnSwapChain — now compiled from video.cpp
// GameWindow::Update, GetTitle, IsFullscreen, etc. — now compiled from game_window.cpp
bool InstallerWizard::Run(std::filesystem::path, bool) { return false; }
float Scale(float v, bool) { return v; }
void SetShaderModifier(unsigned int) {}
void PlayerLimitPatches::Init(void) {}
ImFont* ImFontAtlasSnapshot::GetFont(const char*) { return nullptr; }
void DrawTextWithShadow(const ImFont*, float, const ImVec2&, unsigned int, const char*, float, float, unsigned int) {}
double ComputeMotion(double a, double b, double c, bool d) { (void)a; (void)b; (void)c; (void)d; return 0.0; }
float Hermite(float a, float b, float c) { (void)a; (void)b; (void)c; return 0.0f; }
double ComputeLinearMotion(double a, double b, double c, bool d) { (void)a; (void)b; (void)c; (void)d; return 0.0; }
float Lerp(float a, float b, float c) { (void)a; (void)b; (void)c; return a; }
void DrawVersionString(unsigned int) {}
void ResetGradient(void) {}
void SetHorizontalGradient(const ImVec2&, const ImVec2&, unsigned int, unsigned int) {}
void SetVerticalGradient(const ImVec2&, const ImVec2&, unsigned int, unsigned int) {}
ImFont* g_pFntNewRodin = nullptr;
ImFont* g_pFntRodin = nullptr;
// Xex2LoadImage now compiled from tools/XenonRecomp/XenonUtils/xex.cpp

// ---- GTA V PPC function stubs ---------------------------------------------------
// These functions are declared with __declspec(dllimport) in SanRecompLib
// but were not recompiled. Provide empty stubs.
extern "C" {
void _sub_829D1758(void) {}
void _sub_829D8860(void) {}
}

// ---- PPC Kernel stubs ---------------------------------------------------
// These are declared in ppc_recomp_shared.h with C linkage. Definitions here.
void __imp__XamBackgroundDownloadGetMode(PPCContext&, uint8_t*) {}
void __imp__XamBackgroundDownloadSetMode(PPCContext&, uint8_t*) {}
void __imp__XamContentResolve(PPCContext&, uint8_t*) {}
void __imp__XamMarketplaceAcquireFreeContent(PPCContext&, uint8_t*) {}
void __imp__XamShowKeyboardUI(PPCContext&, uint8_t*) {}
void __imp__XamShowMarketplaceUI(PPCContext&, uint8_t*) {}
void __imp__XamShowMessageComposeUI(PPCContext&, uint8_t*) {}
void __imp__XamShowFriendRequestUI(PPCContext&, uint8_t*) {}
void __imp__XamShowMarketplaceDownloadItemsUI(PPCContext&, uint8_t*) {}
void __imp__XamBackgroundDownloadItemGetStatus(PPCContext&, uint8_t*) {}
void __imp__XamBackgroundDownloadItemGetHistoryStatus(PPCContext&, uint8_t*) {}
void __imp__XamUserGetAge(PPCContext&, uint8_t*) {}
void __imp__XamUserGetAgeGroup(PPCContext&, uint8_t*) {}
void __imp__ExAllocatePool(PPCContext&, uint8_t*) {}
void __imp__MmMapIoSpace(PPCContext&, uint8_t*) {}
void __imp__NtWriteFileGather(PPCContext&, uint8_t*) {}
void __imp__XAudioEnableDucker(PPCContext&, uint8_t*) {}
void __imp__XAudioGetDuckerAttackTime(PPCContext&, uint8_t*) {}
void __imp__XAudioGetDuckerHoldTime(PPCContext&, uint8_t*) {}
void __imp__XAudioGetDuckerLevel(PPCContext&, uint8_t*) {}
void __imp__XAudioGetDuckerReleaseTime(PPCContext&, uint8_t*) {}
void __imp__XAudioGetDuckerThreshold(PPCContext&, uint8_t*) {}
void __imp__XexLoadImage(PPCContext&, uint8_t*) {}
void __imp__XexUnloadImage(PPCContext&, uint8_t*) {}
void __imp__KeInitializeDpc(PPCContext&, uint8_t*) {}
void __imp__KeInsertQueueDpc(PPCContext&, uint8_t*) {}
void __imp__KeSetCurrentProcessType(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpCloseHandle(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpConnect(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpDoWork(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpOpen(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpOpenRequest(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpQueryHeaders(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpReadData(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpReceiveResponse(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpSendRequest(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpSetStatusCallback(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpShutdown(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpStartup(PPCContext&, uint8_t*) {}
void __imp__NetDll_XHttpWriteData(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetConnect(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetDnsLookup(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetDnsRelease(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetGetOpt(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetInAddrToServer(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetInAddrToXnAddr(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetRegisterKey(PPCContext&, uint8_t*) {}
void __imp__NetDll_XNetUnregisterKey(PPCContext&, uint8_t*) {}
void __imp__NetDll_XnpLogonGetStatus(PPCContext&, uint8_t*) {}
void __imp__NtYieldExecution(PPCContext&, uint8_t*) {}
void __imp__VdSetDisplayModeOverride(PPCContext&, uint8_t*) {}
void __imp__XamFreeToken(PPCContext&, uint8_t*) {}
void __imp__XamGetLanguage(PPCContext&, uint8_t*) {}
void __imp__XamGetToken(PPCContext&, uint8_t*) {}
void __imp__XampXAuthGetTitleBuffer(PPCContext&, uint8_t*) {}
void __imp__XampXAuthShutdown(PPCContext&, uint8_t*) {}
void __imp__XampXAuthStartup(PPCContext&, uint8_t*) {}
void __imp__XamSwapCancel(PPCContext&, uint8_t*) {}
void __imp__XamSwapDisc(PPCContext&, uint8_t*) {}
void __imp__XamUserGetDeviceContext(PPCContext&, uint8_t*) {}
void __imp__XamUserGetMembershipTierFromXUID(PPCContext&, uint8_t*) {}
void __imp__XamUserGetOnlineCountryFromXUID(PPCContext&, uint8_t*) {}
void __imp__XamVoiceIsActiveProcess(PPCContext&, uint8_t*) {}
void __imp__XNetLogonGetMachineID(PPCContext&, uint8_t*) {}
void __imp__XNetLogonGetTitleID(PPCContext&, uint8_t*) {}
