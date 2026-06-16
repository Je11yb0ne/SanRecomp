#pragma once
#include <windows.h>
#include <cstdio>

struct PPCContext;
extern PPCContext* g_ppcContext;

// Crash handler for PPC guest thread.
// 1. WRITE faults on the function table region → unprotect page and retry
// 2. EXECUTE faults on corrupted function pointers → attempt recovery via LR
struct ScopedCrashGuard {
    static uint8_t* s_base;
    static constexpr uint64_t TABLE_START = 0x83DC0000ull;
    static constexpr uint64_t TABLE_END   = 0x86871000ull;

    static LONG WINAPI Handler(EXCEPTION_POINTERS* info) {
        DWORD code = info->ExceptionRecord->ExceptionCode;

        if (code == EXCEPTION_ACCESS_VIOLATION) {
            ULONG_PTR fa = info->ExceptionRecord->ExceptionInformation[1];
            DWORD op = (DWORD)info->ExceptionRecord->ExceptionInformation[0];

            // WRITE fault in function table region: unprotect the page and retry
            if (op == 1 && s_base && fa >= (ULONG_PTR)(s_base + TABLE_START)
                && fa < (ULONG_PTR)(s_base + TABLE_END)) {
                ULONG_PTR page = fa & ~0xFFFull;
                static int s_unprotect_count = 0;
                if (++s_unprotect_count <= 10) {
                    printf("[CRASH] Unprotecting fn table page at PPC=0x%llX\n",
                        (unsigned long long)(fa - (ULONG_PTR)s_base));
                    fflush(stdout);
                }
                VirtualProtect((void*)page, 0x1000, PAGE_READWRITE, &code);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // EXECUTE fault: corrupted function pointer
            if (op == 8 && g_ppcContext && g_ppcContext->lr != 0 && s_base) {
                uint32_t ppc_lr = (uint32_t)g_ppcContext->lr;
                if (ppc_lr >= 0x82210000 && ppc_lr < 0x83770000) {
                    const uint64_t CODE_BASE = 0x82210000ull;
                    const uint64_t IMG_BASE  = 0x82000000ull;
                    const uint64_t IMG_SIZE  = 0x1DC0000ull;
                    typedef void PPCFunc(struct PPCContext&, uint8_t*);
                    uint64_t offset = (uint64_t)(ppc_lr - CODE_BASE) * 2;
                    PPCFunc** slot = (PPCFunc**)(s_base + IMG_BASE + IMG_SIZE + offset);
                    PPCFunc* target = *slot;
                    if (target && target != (PPCFunc*)(-1)) {
                        printf("[RECOVER] Execute fault → redirect to PPC LR=0x%08X (host=%p)\n",
                            ppc_lr, (void*)target);
                        fflush(stdout);
                        g_ppcContext->r3.s64 = -1;
#ifdef _WIN64
                        info->ContextRecord->Rip = (DWORD64)target;
#else
                        info->ContextRecord->Eip = (DWORD)target;
#endif
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
            }

            printf("[CRASH] Exception 0x%08lX at IP=%p\n", code, info->ExceptionRecord->ExceptionAddress);
            printf("[CRASH] %s at addr 0x%p\n",
                op == 0 ? "READ FAULT" : (op == 1 ? "WRITE FAULT" : "EXECUTE FAULT"),
                (void*)fa);
        } else {
            printf("[CRASH] Exception 0x%08lX at IP=%p\n", code, info->ExceptionRecord->ExceptionAddress);
        }

        if (g_ppcContext) {
            printf("[CRASH] PPC state: r1=0x%08llX r3=0x%08llX lr=0x%08llX\n",
                (unsigned long long)g_ppcContext->r1.u64,
                (unsigned long long)g_ppcContext->r3.u64,
                (unsigned long long)g_ppcContext->lr);
        }
        fflush(stdout);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    PVOID handle = nullptr;
    ScopedCrashGuard() { handle = AddVectoredExceptionHandler(1, Handler); }
    ~ScopedCrashGuard() { if (handle) RemoveVectoredExceptionHandler(handle); }
};
uint8_t* ScopedCrashGuard::s_base = nullptr;
