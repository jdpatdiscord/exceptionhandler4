#include <phnt.h>

#include <stdio.h>

#define EH4_SOURCE_FILE // Read by eh4.h to not include Windows.h.
#include <eh4.h>

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

typedef NTSTATUS(NTAPI* T_NtSuspendProcess)(HANDLE Process);
T_NtSuspendProcess g_pfnNtSuspendProcess = NULL;

LPTOP_LEVEL_EXCEPTION_FILTER g_pfnPreviousFilter = NULL;
PEXCEPTION_POINTERS g_pStoredExceptionInformation = NULL;
HANDLE g_hExceptionHandlerWatchdogProcess = INVALID_HANDLE_VALUE;
HANDLE g_hCrashNotificationEvent = INVALID_HANDLE_VALUE;
HANDLE g_hCrashNotificationEventHandle = INVALID_HANDLE_VALUE;
HANDLE g_hStartedEvent = INVALID_HANDLE_VALUE;
HANDLE g_hStartedEventHandle = INVALID_HANDLE_VALUE;
HANDLE g_hCurrentProcess = INVALID_HANDLE_VALUE;
HANDLE g_hWatchdogThread = INVALID_HANDLE_VALUE;
BOOL g_bStopFlag = FALSE;

LONG EH4_UnhandledExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    DWORD Code = ExceptionInfo->ExceptionRecord->ExceptionCode;

    g_pStoredExceptionInformation = ExceptionInfo;

    SetEvent(g_hCrashNotificationEvent);

    g_pfnNtSuspendProcess(g_hCurrentProcess);

    return EXCEPTION_EXECUTE_HANDLER;
}

ULONG EH4_AgentWatchdogProc(LPVOID Argument)
{
    WCHAR szCmdLine[512];

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    while (1)
    {
        WaitForSingleObject(g_hExceptionHandlerWatchdogProcess, INFINITE);

        DebugPrint(L"[EH4] Watchdog died, restarting it now");

        if (g_bStopFlag)
        {
            return 0;
        }

        swprintf_s(szCmdLine, _countof(szCmdLine), 
            L"eh4_watchdog.exe %p %p %p %p", 
            g_hCurrentProcess, 
            g_hCrashNotificationEventHandle, 
            g_hStartedEventHandle, 
            &g_pStoredExceptionInformation
        );

        if (!CreateProcessW(
            NULL,
            szCmdLine,
            NULL,
            NULL,
            TRUE,     // inherit handles
            0,
            NULL,
            NULL,
            &si,
            &pi))
        {
            CloseHandle(g_hStartedEventHandle);
            CloseHandle(g_hCrashNotificationEventHandle);
            CloseHandle(g_hCurrentProcess);
            CloseHandle(g_hStartedEvent);
            CloseHandle(g_hCrashNotificationEvent);
            DebugPrint(L"[EH4] Could not restart watchdog");
            return 1;
        }

        g_hExceptionHandlerWatchdogProcess = pi.hProcess;

        CloseHandle(pi.hThread);
    }
}

int EH4_Attach(PEH4_SETTINGS Settings)
{
    WCHAR szCmdLine[512];

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HMODULE hNtDll = GetModuleHandleW(L"ntdll");

    if (!hNtDll)
    {
        DebugPrint(L"[EH4] Could not find ntdll.dll");
        return 1;
    }

    g_pfnNtSuspendProcess = (T_NtSuspendProcess)GetProcAddress(hNtDll, "NtSuspendProcess");
    if (!g_pfnNtSuspendProcess)
    {
        DebugPrint(L"[EH4] Could not find NtSuspendProcess");
        return 2;
    }

    g_hCrashNotificationEvent = CreateEventW(&sa, FALSE, FALSE, NULL);
    if (!g_hCrashNotificationEvent)
    {
        DebugPrint(L"[EH4] Could not CreateEventW (crash notification event)");
        return 3;
    }

    g_hStartedEvent = CreateEventW(&sa, FALSE, FALSE, NULL);
    if (!g_hStartedEvent)
    {
        CloseHandle(g_hCrashNotificationEvent);
        DebugPrint(L"[EH4] Could not CreateEventW (start event)");
        return 4;
    }

    if (!DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentProcess(),
        GetCurrentProcess(),
        &g_hCurrentProcess,
        PROCESS_ALL_ACCESS,
        TRUE,
        0))
    {
        CloseHandle(g_hStartedEvent);
        CloseHandle(g_hCrashNotificationEvent);
        DebugPrint(L"[EH4] Could not DuplicateHandle for watchdog (current process)");
        return 5;
    }

    if (!DuplicateHandle(
        GetCurrentProcess(),
        g_hCrashNotificationEvent,
        GetCurrentProcess(),
        &g_hCrashNotificationEventHandle,
        EVENT_ALL_ACCESS,
        TRUE,
        0))
    {
        CloseHandle(g_hCurrentProcess);
        CloseHandle(g_hStartedEvent);
        CloseHandle(g_hCrashNotificationEvent);
        DebugPrint(L"[EH4] Could not DuplicateHandle for watchdog (crash notification event)");
        return 6;
    }

    if (!DuplicateHandle(
        GetCurrentProcess(),
        g_hStartedEvent,
        GetCurrentProcess(),
        &g_hStartedEventHandle,
        EVENT_ALL_ACCESS,
        TRUE,
        0))
    {
        CloseHandle(g_hCrashNotificationEventHandle);
        CloseHandle(g_hCurrentProcess);
        CloseHandle(g_hStartedEvent);
        CloseHandle(g_hCrashNotificationEvent);
        DebugPrint(L"[EH4] Could not DuplicateHandle for watchdog (start event)");
        return 7;
    }

    swprintf_s(szCmdLine, _countof(szCmdLine),
        L"eh4_watchdog.exe %p %p %p %p",
        g_hCurrentProcess, 
        g_hCrashNotificationEventHandle, 
        g_hStartedEventHandle, 
        &g_pStoredExceptionInformation
    );

    if (!CreateProcessW(
        NULL,
        szCmdLine,
        NULL,
        NULL,
        TRUE,     // inherit handles
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        CloseHandle(g_hStartedEventHandle);
        CloseHandle(g_hCrashNotificationEventHandle);
        CloseHandle(g_hCurrentProcess);
        CloseHandle(g_hStartedEvent);
        CloseHandle(g_hCrashNotificationEvent);
        DebugPrint(L"[EH4] Could not CreateProcessW for watchdog");
        return 8;
    }

    g_hExceptionHandlerWatchdogProcess = pi.hProcess;

    CloseHandle(pi.hThread);

    g_bStopFlag = FALSE;

    g_hWatchdogThread = CreateThread(NULL, 0, EH4_AgentWatchdogProc, NULL, 0, NULL);
    if (g_hWatchdogThread == NULL)
    {
        TerminateProcess(g_hExceptionHandlerWatchdogProcess, 0);
        CloseHandle(g_hExceptionHandlerWatchdogProcess);
        CloseHandle(g_hStartedEventHandle);
        CloseHandle(g_hCrashNotificationEventHandle);
        CloseHandle(g_hCurrentProcess);
        CloseHandle(g_hStartedEvent);
        CloseHandle(g_hCrashNotificationEvent);
        DebugPrint(L"[EH4] Could not CreateThread for watchdog");
        return 9;
    }

    DebugPrint(L"[EH4] Waiting on watchdog to say hello");

    WaitForSingleObject(g_hStartedEvent, INFINITE);

    DebugPrint(L"[EH4] WaitForSingleObject resumed hStartedEvent");

    g_pfnPreviousFilter = SetUnhandledExceptionFilter(EH4_UnhandledExceptionHandler);

    return 0;
}

int EH4_Detach()
{
    g_bStopFlag = TRUE;

    TerminateProcess(g_hExceptionHandlerWatchdogProcess, 0);
    WaitForSingleObject(g_hWatchdogThread, INFINITE);
    CloseHandle(g_hExceptionHandlerWatchdogProcess);
    CloseHandle(g_hStartedEventHandle);
    CloseHandle(g_hCrashNotificationEventHandle);
    CloseHandle(g_hCurrentProcess);
    CloseHandle(g_hStartedEvent);
    CloseHandle(g_hCrashNotificationEvent);

    SetUnhandledExceptionFilter(g_pfnPreviousFilter);

    return 0;
}