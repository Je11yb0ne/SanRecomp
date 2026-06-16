#pragma once
#include <windows.h>
#include <cstdio>

// Forward declare — actual definition in guest_thread.h
struct PPCContext;
extern PPCContext* g_ppcContext;

// Inline crash handler registration for PPC guest thread debugging
struct ScopedCrashGuard {
    static LONG WINAPI Handler(EXCEPTION_POINTERS* info) {
        DWORD code = info->ExceptionRecord->ExceptionCode;
        void* ip = info->ExceptionRecord->ExceptionAddress;
        printf("[CRASH] Exception 0x%08lX at IP=%p\n", code, ip);
        if (code == EXCEPTION_ACCESS_VIOLATION) {
            ULONG_PTR fa = info->ExceptionRecord->ExceptionInformation[1];
            DWORD op = (DWORD)info->ExceptionRecord->ExceptionInformation[0];
            printf("[CRASH] %s at addr 0x%p\n",
                op == 0 ? "READ FAULT" : (op == 1 ? "WRITE FAULT" : "EXECUTE FAULT"),
                (void*)fa);
        }
        // Print PPC context if available
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
