// Standalone STFS/PIRS package extractor using rexglue's StfsContainerDevice.
// Extracts the GTA V 8GB install packages into a loose file tree that the
// game's game:\xbox360\... / game:\common\... lookups can resolve.
//
// Usage:
//   stfs_extract <package_file> <target_root>
//   stfs_extract --list <package_file>          (list entries, no extract)
#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <queue>
#include <span>
#include <string>

namespace fs = std::filesystem;
using rex::X_STATUS;  // X_STATUS_SUCCESS macro casts to unqualified X_STATUS
using rex::filesystem::Entry;
using rex::filesystem::File;
using rex::filesystem::StfsContainerDevice;

static uint64_t g_files = 0;
static uint64_t g_bytes = 0;

static bool ExtractEntry(Entry* entry, const fs::path& base, bool list_only) {
    // Skip the synthetic root (empty path).
    if (entry->path().empty()) return true;

    std::string rel = entry->path();
    for (auto& c : rel) if (c == '\\') c = '/';
    fs::path dest = base / fs::path(rel);

    if (entry->attributes() & rex::filesystem::kFileAttributeDirectory) {
        if (list_only) {
            printf("[DIR ] %s\n", entry->path().c_str());
        } else {
            std::error_code ec;
            fs::create_directories(dest, ec);
        }
        return true;
    }

    if (list_only) {
        printf("[FILE] %s (%llu bytes)\n", entry->path().c_str(),
               (unsigned long long)entry->size());
        ++g_files;
        return true;
    }

    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);

    File* in = nullptr;
    if (entry->Open((uint32_t)rex::filesystem::FileAccess::kFileReadData, &in) !=
            X_STATUS_SUCCESS || !in) {
        printf("  ! open failed: %s\n", entry->path().c_str());
        return false;
    }

    FILE* out = fopen(dest.string().c_str(), "wb");
    if (!out) { in->Destroy(); printf("  ! create failed: %s\n", dest.string().c_str()); return false; }

    constexpr size_t kBuf = 4 * 1024 * 1024;
    auto buf = std::make_unique<uint8_t[]>(kBuf);
    size_t remaining = entry->size();
    size_t offset = 0;
    while (remaining > 0) {
        size_t to_read = remaining < kBuf ? remaining : kBuf;
        size_t got = 0;
        in->ReadSync(std::span<uint8_t>(buf.get(), to_read), offset, &got);
        if (got == 0) break;
        fwrite(buf.get(), 1, got, out);
        offset += got;
        remaining -= got;
        g_bytes += got;
    }
    fclose(out);
    in->Destroy();
    ++g_files;
    if ((g_files % 200) == 0)
        printf("  ... %llu files, %.1f MB\n", (unsigned long long)g_files, g_bytes / 1048576.0);
    return true;
}

int main(int argc, char** argv) {
    rex::LogConfig logcfg;
    logcfg.default_level = spdlog::level::warn;
    logcfg.log_to_console = true;
    rex::InitLogging(logcfg);

    bool list_only = false;
    int ai = 1;
    if (argc > 1 && std::string(argv[1]) == "--list") { list_only = true; ai = 2; }
    if (argc < ai + 1) {
        fprintf(stderr, "Usage: stfs_extract [--list] <package_file> [target_root]\n");
        return 2;
    }
    fs::path package = argv[ai];
    fs::path target = (argc > ai + 1) ? fs::path(argv[ai + 1]) : fs::path(".");

    printf("=== STFS extract ===\npackage: %s\n", package.string().c_str());
    if (!list_only) printf("target : %s\n", target.string().c_str());

    auto device = std::make_unique<StfsContainerDevice>("", package);
    if (!device->Initialize()) {
        fprintf(stderr, "FAIL: StfsContainerDevice::Initialize\n");
        return 1;
    }
    Entry* root = device->ResolvePath("");
    if (!root) { fprintf(stderr, "FAIL: ResolvePath(\"\")\n"); return 1; }

    std::queue<Entry*> q;
    q.push(root);
    while (!q.empty()) {
        Entry* e = q.front(); q.pop();
        for (auto& ch : e->children()) q.push(ch.get());
        if (!ExtractEntry(e, target, list_only)) return 1;
    }

    printf("DONE: %llu files, %.1f MB\n", (unsigned long long)g_files, g_bytes / 1048576.0);
    return 0;
}
