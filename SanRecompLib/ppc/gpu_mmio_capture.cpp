// GPU MMIO Capture — intercepts GPU register writes from PPC code
// Called from _gpu_mmio_store32() in ppc_config.h

#include <cstdint>
#include <cstdio>
#include <atomic>

// Ring buffer for captured GPU register writes
struct GpuWrite {
    uint32_t reg;   // register index (dword offset from GPU_MMIO_BASE)
    uint32_t val;   // value written
};

static constexpr int kCaptureSize = 65536;
static GpuWrite s_captureBuffer[kCaptureSize];
static std::atomic<uint32_t> s_captureWritePtr{0};
static std::atomic<bool> s_dumpEnabled{true};

void CaptureGpuWrite(uint32_t reg, uint32_t val) {
    uint32_t idx = s_captureWritePtr.fetch_add(1) % kCaptureSize;
    s_captureBuffer[idx].reg = reg;
    s_captureBuffer[idx].val = val;
}

// Called from VBlank or debug code to dump captured GPU commands
void DumpGpuCapture() {
    uint32_t count = s_captureWritePtr.load();
    if (count == 0) {
        printf("[GPU-CAP] No GPU MMIO writes captured\n");
        return;
    }
    // Dump all captured writes (up to 200)
    uint32_t dumpCount = count > 200 ? 200u : count;
    printf("[GPU-CAP] %u total GPU MMIO writes (showing first %u):\n", count, dumpCount);
    for (uint32_t i = 0; i < dumpCount; i++) {
        uint32_t idx = i % kCaptureSize;
        uint32_t r = s_captureBuffer[idx].reg;
        uint32_t v = s_captureBuffer[idx].val;
        const char* tag = "";
        if (r == 0x1844) tag = " *** D1GRPH_PRIMARY_SURFACE_ADDRESS ***";
        else if (r == 0x1841) tag = " - D1GRPH_CONTROL";
        else if (r == 0x1852) tag = " - D1GRPH_FLIP_CONTROL";
        else if (r == 0x1838) tag = " - D1MODE_MASTER_UPDATE_LOCK";
        else if (r == 0x0C00) tag = " - CP_RB_BASE";
        else if (r == 0x0C01) tag = " - CP_RB_CNTL";
        printf("  [%3u] reg=0x%04X val=0x%08X%s\n", i, r, v, tag);
    }
    fflush(stdout);
    // Keep buffer for subsequent dumps
}
