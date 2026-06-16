#pragma once
#include <windows.h>
#include <cstdio>

struct PPCContext;
extern PPCContext* g_ppcContext;

struct ScopedCrashGuard {
    static uint8_t* s_base;
    static constexpr uint64_t TABLE_START = 0x83DC0000ull;
    static constexpr uint64_t TABLE_END   = 0x86871000ull;

    static LONG WINAPI Handler(EXCEPTION_POINTERS* info) {
        DWORD code = info->ExceptionRecord->ExceptionCode;

        if (code == 0xE06D7363 || code == 0xC00000FD) {
            if (g_ppcContext) g_ppcContext->r3.s64 = -1;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (code == EXCEPTION_ACCESS_VIOLATION) {
            ULONG_PTR fa = info->ExceptionRecord->ExceptionInformation[1];
            DWORD op = (DWORD)info->ExceptionRecord->ExceptionInformation[0];

            // WRITE fault in fn table: unprotect page and retry
            if (op == 1 && s_base && fa >= (ULONG_PTR)(s_base + TABLE_START)
                && fa < (ULONG_PTR)(s_base + TABLE_END)) {
                VirtualProtect((void*)(fa & ~0xFFFull), 0x1000, PAGE_READWRITE, &code);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            // Any other access violation: force-commit the page and retry
            VirtualAlloc((void*)(fa & ~0xFFFull), 0x1000, MEM_COMMIT, PAGE_READWRITE);
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (g_ppcContext) {
            printf("[CRASH] Exception 0x%08lX r1=0x%08llX r3=0x%08llX lr=0x%08llX\n",
                code,
                (unsigned long long)g_ppcContext->r1.u64,
                (unsigned long long)g_ppcContext->r3.u64,
                (unsigned long long)g_ppcContext->lr);
            fflush(stdout);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    PVOID handle = nullptr;
    ScopedCrashGuard() { handle = AddVectoredExceptionHandler(1, Handler); }
    ~ScopedCrashGuard() { if (handle) RemoveVectoredExceptionHandler(handle); }
};
uint8_t* ScopedCrashGuard::s_base = nullptr;
