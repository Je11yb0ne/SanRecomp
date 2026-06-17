// Minimal compile verification for rexglue-generated GTA V code
// This test only verifies compilation + linking, not actual execution

#include "generated/default/gta5_recomp_init.h"

// PPCImageConfig and PPCFuncMappings are already defined in the generated init.cpp.
// The init.h declares them as extern — we just use them here.

#include <cstdio>

int main() {
    printf("=== rexglue GTA V compile test ===\n");
    printf("IMAGE_BASE: 0x%08llX\n", (unsigned long long)REX_IMAGE_BASE);
    printf("IMAGE_SIZE: 0x%08llX\n", (unsigned long long)REX_IMAGE_SIZE);
    printf("CODE_BASE:  0x%08llX\n", (unsigned long long)REX_CODE_BASE);
    printf("CODE_SIZE:  0x%08llX\n", (unsigned long long)REX_CODE_SIZE);

    // Count function mappings
    int count = 0;
    for (int i = 0; PPCFuncMappings[i].guest != 0; i++) count++;
    printf("Function mappings: %d\n", count);

    // Verify entry point exists
    for (int i = 0; PPCFuncMappings[i].guest != 0; i++) {
        if (PPCFuncMappings[i].guest == 0x83639888) {
            printf("Entry point 0x83639888 → xstart: FOUND\n");
            break;
        }
    }

    printf("=== Compile verification PASSED ===\n");
    return 0;
}
