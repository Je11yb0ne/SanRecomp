// RPF7 (GTA V, Xbox 360) archive reader / decode validator.
//
// Validates the full on-disk decode pipeline BEFORE porting it into the
// rexglue RpfContainerDevice:
//   - Header parse (big-endian; 28-bit filenamesLength + 3-bit shift).
//   - TOC decrypt: SINGLE AES-256-ECB pass (360; PC uses 16).
//   - Entry tree walk -> full paths.
//   - Per-file decode: read compressedSize bytes @ offset<<9, AES-decrypt if
//     encrypted, chunked LZX-decompress (window=16, shared state) to
//     uncompressedSize. Verifies LZX returns OK + exact size + sane magic.
//
// Usage:
//   rpf_read <archive.rpf> <key.dat>                 list + auto-validate one file
//   rpf_read <archive.rpf> <key.dat> --list          dump every path
//   rpf_read <archive.rpf> <key.dat> --find <substr> [--out <file>]
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "rpf_lzx.h"
#include "rpf_aes.h"

static uint32_t be32(const uint8_t* p) { return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]; }
static uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0]<<8)|p[1]); }

// AES-256-ECB single-pass decrypt of whole 16-byte blocks in place (one pass),
// using the SAME bundled tiny-aes the device uses (validates the device path).
static bool aes256_ecb(uint8_t* buf, size_t len, const uint8_t key[32]) {
    struct AES_ctx ctx;
    RpfAes_init_ctx(&ctx, key);
    size_t whole = len & ~(size_t)15;
    for (size_t off = 0; off < whole; off += 16) RpfAes_ECB_decrypt(&ctx, buf + off);
    return true;
}

// Chunked LZX (360 RPF7). Mirrors refs/RPF7-master/RPF7Console/xbox360.cpp.
// out must hold at least outsize + 0x8000 (padding so a final full chunk can't
// overflow). Returns produced byte count; sets *ok=false on any LZX error.
static int lzx_chunked(const uint8_t* in, int insize, uint8_t* out, int outsize, bool* ok) {
    *ok = true;
    LZXstate* st = LZXinit(16);
    if (!st) { *ok = false; return 0; }
    int produced = 0, offset = 0;
    while (produced < outsize) {
        if (offset + 2 > insize) { *ok = false; break; }
        int tmpout, tmpin;
        if (in[offset] == 0xFF) {
            if (offset + 5 > insize) { *ok = false; break; }
            tmpout = (in[offset+1]<<8) | in[offset+2];
            tmpin  = (in[offset+3]<<8) | in[offset+4];
            offset += 5;
        } else {
            tmpout = 0x8000;
            tmpin  = (in[offset]<<8) | in[offset+1];
            if (tmpin == 0) break;
            offset += 2;
        }
        if (offset + tmpin > insize) { *ok = false; break; }
        int res = LZXdecompress(st, (unsigned char*)(in + offset), out + produced, tmpin, tmpout);
        if (res != 0) { printf("  [LZX] error %d at in-offset %d (chunk out=%d in=%d)\n",
                               res, offset, tmpout, tmpin); *ok = false; break; }
        offset += tmpin;
        produced += tmpout;
    }
    LZXteardown(st);
    return produced;
}

struct Node {
    std::string name;
    bool isDir = false, isResource = false, isEncrypted = false;
    uint64_t dataOffset = 0;
    uint32_t compressedSize = 0, uncompressedSize = 0;
    uint32_t subIndex = 0, subCount = 0;
};

static std::vector<Node> g_nodes;
static std::vector<std::pair<std::string, uint32_t>> g_files;  // (path, node index)
static std::vector<std::pair<std::string, uint32_t>> g_dirs;

static void walk(uint32_t idx, const std::string& parent, int depth) {
    if (idx >= g_nodes.size() || depth > 64) return;
    const Node& n = g_nodes[idx];
    std::string path = (parent.empty() || n.name.empty())
                       ? (parent + n.name) : (parent + "/" + n.name);
    if (n.isDir) {
        g_dirs.push_back({path, idx});
        for (uint32_t i = 0; i < n.subCount; ++i)
            walk(n.subIndex + i, path, depth + 1);
    } else {
        g_files.push_back({path, idx});
    }
}

// Read + fully decode one file node. Returns decoded bytes (empty on failure).
static std::vector<uint8_t> decodeFile(FILE* f, const Node& n, const uint8_t key[32]) {
    uint32_t readSize = n.compressedSize ? n.compressedSize : n.uncompressedSize;
    std::vector<uint8_t> raw(readSize);
    _fseeki64(f, (long long)n.dataOffset, SEEK_SET);
    if (fread(raw.data(), 1, readSize, f) != readSize) { printf("  read failed\n"); return {}; }

    if (n.isEncrypted) aes256_ecb(raw.data(), raw.size(), key);

    if (n.compressedSize == 0) return raw;  // stored uncompressed

    std::vector<uint8_t> out(n.uncompressedSize + 0x8000);
    bool ok = false;
    int produced = lzx_chunked(raw.data(), (int)raw.size(), out.data(), (int)n.uncompressedSize, &ok);
    if (!ok || produced != (int)n.uncompressedSize) {
        printf("  DECODE FAIL: produced=%d expected=%u ok=%d\n", produced, n.uncompressedSize, ok);
        return {};
    }
    out.resize(n.uncompressedSize);
    return out;
}

static void dumpHead(const std::vector<uint8_t>& d) {
    int show = (int)(d.size() < 48 ? d.size() : 48);
    printf("  first %d bytes: ", show);
    for (int i = 0; i < show; ++i) printf("%02X ", d[i]);
    printf("\n  ascii: ");
    for (int i = 0; i < show; ++i) { uint8_t c = d[i]; putchar((c>=32&&c<127)?c:'.'); }
    printf("\n");
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: rpf_read <archive.rpf> <key.dat> [--list|--find <substr>] [--out <file>]\n"); return 2; }
    const char* archivePath = argv[1];
    const char* keyPath = argv[2];
    bool doList = false; std::string findSub, outPath;
    for (int i = 3; i < argc; ++i) {
        if (!strcmp(argv[i], "--list")) doList = true;
        else if (!strcmp(argv[i], "--find") && i+1 < argc) findSub = argv[++i];
        else if (!strcmp(argv[i], "--out") && i+1 < argc) outPath = argv[++i];
    }

    FILE* f = fopen(archivePath, "rb");
    if (!f) { fprintf(stderr, "open archive failed\n"); return 1; }
    uint8_t hdr[16];
    if (fread(hdr, 1, 16, f) != 16 || memcmp(hdr, "RPF7", 4)) { fprintf(stderr, "not RPF7\n"); fclose(f); return 1; }
    uint32_t entryCount = be32(hdr + 4);
    uint32_t infos      = be32(hdr + 8);
    uint32_t flags      = be32(hdr + 12);
    uint32_t namesLen   = infos & 0x0FFFFFFF;        // 28 bits
    uint32_t shift      = (infos >> 28) & 7;         // 3 bits
    uint8_t  platform   = (infos >> 31) & 1;
    printf("RPF7: entries=%u namesLen=%u shift=%u platform=%u flags=0x%08X\n",
           entryCount, namesLen, shift, platform, flags);

    uint32_t tocSize = entryCount * 16 + namesLen;
    std::vector<uint8_t> toc(tocSize);
    if (fread(toc.data(), 1, tocSize, f) != tocSize) { fprintf(stderr, "toc read failed\n"); fclose(f); return 1; }

    uint8_t key[32];
    FILE* kf = fopen(keyPath, "rb");
    if (!kf || fread(key, 1, 32, kf) != 32) { fprintf(stderr, "key read failed\n"); fclose(f); return 1; }
    fclose(kf);

    bool isOpen = (flags == 0x4E45504F);  // 'OPEN'
    printf("toc %s\n", isOpen ? "plaintext (OPEN)" : "encrypted -> AES-256-ECB x1");
    // Entry table + names blob are contiguous and encrypted together; decrypt
    // the whole TOC region in one single pass (matches balika011 RPF7Console).
    if (!isOpen && !aes256_ecb(toc.data(), tocSize, key)) {
        fprintf(stderr, "toc decrypt failed\n"); fclose(f); return 1;
    }

    const uint8_t* names = toc.data() + entryCount * 16;

    // Parse all entries.
    g_nodes.resize(entryCount);
    for (uint32_t i = 0; i < entryCount; ++i) {
        const uint8_t* e = toc.data() + i * 16;
        Node& n = g_nodes[i];
        uint32_t offRaw = (e[0]<<16) | (e[1]<<8) | e[2];
        n.isResource = (e[0] >> 7) & 1;
        uint32_t off = offRaw & 0x7FFFFF;
        n.compressedSize = (e[3]<<16) | (e[4]<<8) | e[5];
        uint32_t nameOff = be16(e + 6) << shift;
        n.name = (nameOff < namesLen) ? std::string((const char*)names + nameOff) : std::string();
        if (off == 0x7FFFFF) {
            n.isDir = true;
            n.subIndex = be32(e + 8);
            n.subCount = be32(e + 12);
        } else {
            n.dataOffset = (uint64_t)off << 9;
            n.uncompressedSize = be32(e + 8);
            n.isEncrypted = be32(e + 12) == 1;
        }
    }

    walk(0, "", 0);
    printf("parsed: %zu dirs, %zu files\n\n", g_dirs.size(), g_files.size());

    if (doList) {
        for (auto& fp : g_files)
            printf("  %s\n", fp.first.c_str());
        fclose(f); return 0;
    }

    // Print a sample of paths.
    printf("=== sample paths (first 25 files) ===\n");
    for (size_t i = 0; i < g_files.size() && i < 25; ++i)
        printf("  %s\n", g_files[i].first.c_str());
    printf("\n");

    // Choose a file to validate.
    int target = -1;
    if (!findSub.empty()) {
        for (size_t i = 0; i < g_files.size(); ++i)
            if (g_files[i].first.find(findSub) != std::string::npos) { target = (int)i; break; }
        if (target < 0) { printf("no file matching '%s'\n", findSub.c_str()); fclose(f); return 1; }
    } else {
        // auto: first compressed, non-resource file (cleanest decode validation)
        for (size_t i = 0; i < g_files.size(); ++i) {
            const Node& n = g_nodes[g_files[i].second];
            if (n.compressedSize != 0 && !n.isResource) { target = (int)i; break; }
        }
        if (target < 0) { printf("no compressed non-resource file found to validate\n"); fclose(f); return 1; }
    }

    const std::string& tpath = g_files[target].first;
    const Node& tn = g_nodes[g_files[target].second];
    printf("=== validate: %s ===\n", tpath.c_str());
    printf("  offset=0x%llX comp=%u uncomp=%u resource=%d encrypted=%d\n",
           (unsigned long long)tn.dataOffset, tn.compressedSize, tn.uncompressedSize,
           tn.isResource, tn.isEncrypted);

    std::vector<uint8_t> data = decodeFile(f, tn, key);
    fclose(f);
    if (data.empty()) { printf("DECODE FAILED\n"); return 1; }
    printf("  DECODE OK: %zu bytes\n", data.size());
    dumpHead(data);

    if (!outPath.empty()) {
        FILE* o = fopen(outPath.c_str(), "wb");
        if (o) { fwrite(data.data(), 1, data.size(), o); fclose(o); printf("  wrote %s\n", outPath.c_str()); }
    }
    return 0;
}
