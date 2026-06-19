// RPF7 (GTA V) archive TOC reader / validator.
// Decrypts the AES-256 encrypted TOC (RAGE applies AES-256-ECB 16 times) and
// dumps the directory/file entries to validate the key + on-disk format before
// building the rexglue RpfContainerDevice. Xbox 360 = big-endian fields.
//
// Usage: rpf_read <archive.rpf> <key.dat>
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#pragma comment(lib, "bcrypt.lib")

static uint32_t be32(const uint8_t* p) { return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]; }

// AES-256-ECB decrypt the buffer in place, applied `rounds` times (RAGE=16).
static bool aes256_ecb_decrypt_rounds(std::vector<uint8_t>& buf, const uint8_t key[32], int rounds) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB,
                      (ULONG)sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    BCRYPT_KEY_HANDLE hk = nullptr;
    if (BCryptGenerateSymmetricKey(alg, &hk, nullptr, 0, (PUCHAR)key, 32, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0); return false;
    }
    size_t len = buf.size() & ~(size_t)15;  // whole 16-byte blocks
    for (int r = 0; r < rounds; ++r) {
        ULONG done = 0;
        NTSTATUS st = BCryptDecrypt(hk, buf.data(), (ULONG)len, nullptr, nullptr, 0,
                                    buf.data(), (ULONG)len, &done, 0);
        if (st != 0) { printf("BCryptDecrypt failed 0x%lX\n", (long)st); break; }
    }
    BCryptDestroyKey(hk);
    BCryptCloseAlgorithmProvider(alg, 0);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: rpf_read <archive.rpf> <key.dat>\n"); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "open archive failed\n"); return 1; }
    uint8_t hdr[16];
    fread(hdr, 1, 16, f);
    if (hdr[0]!='R'||hdr[1]!='P'||hdr[2]!='F'||hdr[3]!='7') { fprintf(stderr, "not RPF7\n"); fclose(f); return 1; }
    uint32_t entryCount = be32(hdr + 4);
    uint32_t namesField = be32(hdr + 8);
    uint32_t encryption = be32(hdr + 12);
    uint32_t namesLen = namesField & 0x7FFFFFFF;
    printf("RPF7: entryCount=%u namesField=0x%08X (namesLen=%u) encryption=0x%08X\n",
           entryCount, namesField, namesLen, encryption);

    uint32_t tocSize = entryCount * 16 + namesLen;
    std::vector<uint8_t> toc(tocSize);
    fread(toc.data(), 1, tocSize, f);
    fclose(f);

    // Load key
    FILE* kf = fopen(argv[2], "rb");
    uint8_t key[32];
    if (!kf || fread(key, 1, 32, kf) != 32) { fprintf(stderr, "key read failed\n"); return 1; }
    fclose(kf);

    bool isOpen = (encryption == 0x4E45504F); // 'OPEN'
    printf("encryption %s -> %s\n", isOpen ? "OPEN(plaintext)" : "encrypted",
           isOpen ? "no decrypt" : "AES-256-ECB x1 (360 single-pass)");
    if (!isOpen) aes256_ecb_decrypt_rounds(toc, key, 1);  // 360 = single ECB pass

    // Dump first 3 entries (16 bytes each) + start of names blob
    printf("\n=== first entries (16B each, BE) ===\n");
    for (uint32_t i = 0; i < entryCount && i < 6; ++i) {
        const uint8_t* e = toc.data() + i * 16;
        printf("[%u] ", i);
        for (int b = 0; b < 16; ++b) printf("%02X ", e[b]);
        printf("\n");
    }
    const uint8_t* names = toc.data() + entryCount * 16;
    printf("\n=== names blob (first 256 bytes, '.'=nonprintable) ===\n");
    for (uint32_t i = 0; i < namesLen && i < 256; ++i) {
        uint8_t c = names[i];
        putchar((c >= 32 && c < 127) ? c : (c == 0 ? '|' : '.'));
    }
    printf("\n");
    return 0;
}
