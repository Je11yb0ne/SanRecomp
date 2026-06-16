// dump_xex.cpp - Decrypt and decompress XEX to raw binary for IDA analysis
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <vector>
#include <stdint.h>

// Minimal XEX2 structures (from XenonUtils)
struct Xex2Header {
    uint32_t magic;
    uint32_t moduleFlags;
    uint32_t headerSize;
    uint32_t reserved;
    uint32_t securityOffset;
    uint32_t headerCount;
};

struct Xex2SecurityInfo {
    uint32_t headerSize;
    uint32_t imageSize;
    uint8_t rsaSignature[0x100];
    uint32_t padding;
    uint8_t aesKey[0x10];
    uint32_t exportTable;
};

struct Xex2OptFileFormatInfo {
    uint32_t infoSize;
    uint32_t encryptionType;
    uint32_t compressionType;
};

struct Xex2FileBasicCompressionBlock {
    uint32_t dataSize;
    uint32_t zeroSize;
};

// AES is not included here, use rexglue tools or external script instead
int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <xex_path> <output_path>\n", argv[0]);
        fprintf(stderr, "Note: XEX must be unencrypted or have known retail key\n");
        return 1;
    }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen input"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(fsize);
    fread(data.data(), 1, fsize, f);
    fclose(f);
    
    auto* hdr = (Xex2Header*)data.data();
    if (hdr->magic != 0x58455832) { // "XEX2"
        fprintf(stderr, "Not a valid XEX2 file (magic=0x%08X)\n", hdr->magic);
        return 1;
    }
    
    auto* sec = (Xex2SecurityInfo*)(data.data() + hdr->securityOffset);
    auto* fmt = (Xex2OptFileFormatInfo*)(data.data() + hdr->headerSize);
    
    printf("XEX2: headerSize=0x%X securityOffset=0x%X imageSize=%u\n",
           hdr->headerSize, hdr->securityOffset, sec->imageSize);
    printf("Encryption=%u Compression=%u\n", fmt->encryptionType, fmt->compressionType);
    
    if (fmt->encryptionType != 0) {
        fprintf(stderr, "ERROR: XEX is encrypted. Use XenonRecomp or rexglue to decrypt first.\n");
        fprintf(stderr, "Tip: Copy default.xex to rexglue's test_gta5/ and run rexglue codegen.\n");
        return 1;
    }
    
    // For unencrypted XEX, extract raw image
    uint32_t imageBase = 0x82000000; // Default Xbox 360 base
    uint32_t entry = *(uint32_t*)(data.data() + hdr->headerSize + 0x18); // approximate
    
    if (fmt->compressionType == 0) { // None
        FILE* out = fopen(argv[2], "wb");
        fwrite(data.data() + hdr->headerSize, 1, sec->imageSize, out);
        fclose(out);
        printf("Wrote %u bytes (uncompressed) to %s\n", sec->imageSize, argv[2]);
    } else if (fmt->compressionType == 1) { // Basic
        auto* blocks = (Xex2FileBasicCompressionBlock*)(fmt + 1);
        int numBlocks = (fmt->infoSize / 8) - 1;
        printf("Basic compression: %d blocks\n", numBlocks);
        
        std::vector<uint8_t> image(sec->imageSize, 0);
        uint8_t* src = data.data() + hdr->headerSize;
        uint8_t* dst = image.data();
        for (int i = 0; i < numBlocks; i++) {
            memcpy(dst, src, blocks[i].dataSize);
            src += blocks[i].dataSize;
            dst += blocks[i].dataSize;
            dst += blocks[i].zeroSize; // zero-filled BSS
        }
        
        FILE* out = fopen(argv[2], "wb");
        fwrite(image.data(), 1, sec->imageSize, out);
        fclose(out);
        printf("Wrote %u bytes (basic decompressed) to %s\n", sec->imageSize, argv[2]);
    } else {
        fprintf(stderr, "Unknown compression type: %u\n", fmt->compressionType);
        return 1;
    }
    
    printf("Image base: 0x%08X\n", imageBase);
    printf("Entry point (approx): 0x%08X\n", entry);
    
    return 0;
}
