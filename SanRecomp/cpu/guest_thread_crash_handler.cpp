// Crash handler for PPC guest thread — catches access violations and prints fault info
#include <stdafx.h>
#include <windows.h>
#include <cstdio>

static LONG WINAPI GuestThreadCrashHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    void* addr = ExceptionInfo->ExceptionRecord->ExceptionAddress;
    
    printf("[CRASH] Exception 0x%08lX at %p\n", code, addr);
    fflush(stdout);
    
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR faultAddr = ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        DWORD op = (DWORD)ExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        printf("[CRASH] Access violation: %s at address 0x%p\n",
            op == 0 ? "READ" : (op == 1 ? "WRITE" : "EXECUTE"),
            (void*)faultAddr);
        fflush(stdout);
    }
    
    return EXCEPTION_CONTINUE_SEARCH; // Let default handler run
}

static PVOID s_handler = nullptr;

void InstallGuestCrashHandler() {
    if (!s_handler) {
        s_handler = AddVectoredExceptionHandler(1, GuestThreadCrashHandler);
        printf("[CrashHandler] Installed vectored exception handler\n");
        fflush(stdout);
    }
}

void RemoveGuestCrashHandler() {
    if (s_handler) {
        RemoveVectoredExceptionHandler(s_handler);
        s_handler = nullptr;
    }
}
