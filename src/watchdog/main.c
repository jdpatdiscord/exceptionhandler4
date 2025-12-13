#include <phnt.h>

#include <eh4_watchdog.h>

#include <cJSON.h>

void DebugPrint(const wchar_t* format, ...)
{
    static wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, 1024, format, args);
    va_end(args);
    
    OutputDebugStringW(buffer);
    wprintf(L"%s\n", buffer);
}

typedef NTSTATUS(NTAPI* T_NtResumeProcess)(HANDLE Process);
T_NtResumeProcess g_pfnNtResumeProcess = NULL;

void ResumeProcess(HANDLE hProcess)
{
    HANDLE hNtdll = GetModuleHandleW(L"ntdll");
    if (!hNtdll)
    {
        return;
    }

    g_pfnNtResumeProcess = (T_NtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
    if (!g_pfnNtResumeProcess)
    {
        return;
    }

    g_pfnNtResumeProcess(hProcess);
}

int main(int argc, char* argv[])
{
    DebugPrint(L"[Watchdog] External process started");
    if (argc < 6)
    {
        DebugPrint(L"[Watchdog] Not enough arguments, exiting now");
        return 1;
    }

    const char* processHandleStr = argv[1];
    const char* crashEventHandleStr = argv[2];
    const char* startEventHandleStr = argv[3];
    const char* exceptionPtrStr = argv[4];
    const char* callPreviousStr = argv[5];

    char* endParse;

    HANDLE hClientProcess = (HANDLE)strtoull(processHandleStr, &endParse, 16);
    HANDLE hCrashEvent = (HANDLE)strtoull(crashEventHandleStr, &endParse, 16);
    HANDLE hStartEvent = (HANDLE)strtoull(startEventHandleStr, &endParse, 16);
    LPVOID pStoredExceptionInformation = (LPVOID)strtoull(exceptionPtrStr, &endParse, 16);
    BOOLEAN callPreviousHandler = (BOOLEAN)strtoul(callPreviousStr, &endParse, 10);

    SetEvent(hStartEvent);

    HANDLE Handles[2];
    Handles[0] = hCrashEvent;
    Handles[1] = hClientProcess;

    DWORD waitResult = WaitForMultipleObjects(2, Handles, FALSE, INFINITE);

    DWORD index = 0;
    if (waitResult >= WAIT_OBJECT_0 && waitResult <= WAIT_OBJECT_0 + _countof(Handles) - 1)
    {
        index = waitResult - WAIT_OBJECT_0;
    }
    else if (waitResult >= WAIT_ABANDONED_0 && waitResult <= WAIT_ABANDONED_0 + _countof(Handles) - 1)
    {
        index = waitResult - WAIT_ABANDONED_0;
    }
    else
    {
        DebugPrint(L"[Watchdog] Unexpected result from WaitForMultipleObjects: %u", GetLastError());

        return 2;
    }

    // if it wasn't hCrashEvent
    if (index != 0)
    {
        DWORD dwExitCode;
        GetExitCodeProcess(hClientProcess, &dwExitCode);

        DebugPrint(L"[Watchdog] Process exited with code %u; exiting now", dwExitCode);

        CloseHandle(hClientProcess);

        return 0;
    }

    DebugPrint(L"[Watchdog] Received crash notification");

    EH4_ProcessException(hClientProcess, pStoredExceptionInformation);

    if (callPreviousHandler)
    {
        DebugPrint(L"[Watchdog] Resuming process to call previous handler");

        g_pfnNtResumeProcess(hClientProcess);
    }
    else
    {
        DebugPrint(L"[Watchdog] Terminating client process");

        TerminateProcess(hClientProcess, 1);
    }

    CloseHandle(hClientProcess);

    DebugPrint(L"[Watchdog] Watchdog lifecycle is finished, exiting now");

    return 0;
}