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
    uint32_t dumpCount = count > 50 ? 50u : count;
    printf("[GPU-CAP] %u GPU MMIO writes captured (dumping first %u):\n", count, dumpCount);
    for (uint32_t i = 0; i < dumpCount; i++) {
        uint32_t idx = (count - dumpCount + i) % kCaptureSize;
        printf("  [%u] reg=0x%04X val=0x%08X\n", i, s_captureBuffer[idx].reg, s_captureBuffer[idx].val);
    }
    fflush(stdout);
    s_captureWritePtr.store(0);  // Reset after dump
    s_dumpEnabled.store(false);  // Only dump once
}
