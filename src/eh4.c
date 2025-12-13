#include <phnt.h>

#include <stdio.h>

#define EH4_SOURCE_FILE // Read by eh4.h to not include Windows.h.
#include <eh4.h>

#include <SlimDetours.h>

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

typedef NTSTATUS (NTAPI *T_ZwRaiseException)(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance);
T_ZwRaiseException g_pfnZwRaiseException = NULL;

typedef VOID (NTAPI *T_KiUserExceptionDispatcher)(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);
T_KiUserExceptionDispatcher g_pfnKiUserExceptionDispatcher = NULL;

EH4_SETTINGS g_settings;

LPTOP_LEVEL_EXCEPTION_FILTER g_pfnPreviousFilter = NULL;
UINT g_uLastErrorMode = 0;
PEH4_CRASHINFO g_pStoredExceptionInformation = NULL;
HANDLE g_hExceptionHandlerWatchdogProcess = INVALID_HANDLE_VALUE;
HANDLE g_hCrashNotificationEvent = INVALID_HANDLE_VALUE;
HANDLE g_hCrashNotificationEventHandle = INVALID_HANDLE_VALUE;
HANDLE g_hStartedEvent = INVALID_HANDLE_VALUE;
HANDLE g_hStartedEventHandle = INVALID_HANDLE_VALUE;
HANDLE g_hCurrentProcess = INVALID_HANDLE_VALUE;
HANDLE g_hWatchdogThread = INVALID_HANDLE_VALUE;
BOOL g_bStopFlag = FALSE;

EXTERN_C void EH4_KiUserExceptionDispatcher_ASM(void);

void EH4_KiUserExceptionDispatcher_C(PCONTEXT ContextRecord, PEXCEPTION_RECORD ExceptionRecord)
{
    DebugPrint(
        L"[EH4] KiUserExceptionDispatcher: Code=%08X ExceptionAddress=%p rip=%p rsp=%p",
        ExceptionRecord->ExceptionCode,
        ExceptionRecord->ExceptionAddress,
        ContextRecord->Rip,
        ContextRecord->Rsp
        );
    
    return;
}

int EH4_InstallDispatcherHook()
{
    HMODULE hNtDll = GetModuleHandleW(L"ntdll");
    if (!hNtDll)
    {
        return 1;
    }

    g_pfnKiUserExceptionDispatcher = (T_KiUserExceptionDispatcher)GetProcAddress(hNtDll, "KiUserExceptionDispatcher");
    if (!g_pfnKiUserExceptionDispatcher)
    {
        return 2;
    }

    DETOUR_TRANSACTION_OPTIONS Options;
    Options.fSuspendThreads = FALSE;

    HRESULT hr = SlimDetoursTransactionBeginEx(&Options);
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursTransactionBeginEx failed: %08x", hr);
        return 3;
    }

    hr = SlimDetoursAttach((PVOID*)&g_pfnKiUserExceptionDispatcher, EH4_KiUserExceptionDispatcher_ASM);
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursAttach failed: %08x", hr);
        SlimDetoursTransactionAbort();
        return 4;
    }

    hr = SlimDetoursTransactionCommit();
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursTransactionCommit failed: %08x", hr);
        return 5;
    }

    DebugPrint(L"[EH4] Installed KiUserExceptionDispatcher hook\n");

    return 0;
}

// static volatile LONG g_bInHook = FALSE;

NTSTATUS NTAPI EH4_ZwRaiseException_Hook(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance)
{
    // if (InterlockedCompareExchange(&g_bInHook, TRUE, FALSE))
    // {
    //     return g_pfnZwRaiseException(ExceptionRecord, ContextRecord, FirstChance);
    // }

    PVOID ReturnAddress = _ReturnAddress();
    LONG_PTR Distance = (ULONG_PTR)ReturnAddress - (ULONG_PTR)g_pfnKiUserExceptionDispatcher;

    if (Distance >= 0 && Distance < 128)
    {
        EXCEPTION_POINTERS ExceptionPointers;
        ExceptionPointers.ExceptionRecord = ExceptionRecord;
        ExceptionPointers.ContextRecord = ContextRecord;

        EH4_CRASHINFO CrashInfo;
        CrashInfo.pStoredExceptionInformation = &ExceptionPointers;
        CrashInfo.ThreadId = GetCurrentThreadId();

        g_pStoredExceptionInformation = &CrashInfo;

        SetEvent(g_hCrashNotificationEvent);
        g_pfnNtSuspendProcess(g_hCurrentProcess);
    }

    // InterlockedExchange(&g_bInHook, FALSE);

    return g_pfnZwRaiseException(ExceptionRecord, ContextRecord, FirstChance);
}

int EH4_InstallRaiseHook()
{
    HMODULE hNtDll = GetModuleHandleW(L"ntdll");
    if (!hNtDll)
    {
        return 1;
    }

    g_pfnZwRaiseException = (T_ZwRaiseException)GetProcAddress(hNtDll, "ZwRaiseException");
    if (!g_pfnZwRaiseException)
    {
        return 2;
    }

    DETOUR_TRANSACTION_OPTIONS Options;
    Options.fSuspendThreads = FALSE;

    HRESULT hr = SlimDetoursTransactionBeginEx(&Options);
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursTransactionBeginEx failed: %08X", hr);
        return 4;
    }

    hr = SlimDetoursAttach((PVOID*)&g_pfnZwRaiseException, EH4_ZwRaiseException_Hook);
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursAttach failed: %08X", hr);
        SlimDetoursTransactionAbort();
        return 5;
    }

    hr = SlimDetoursTransactionCommit();
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursTransactionCommit failed: %08X", hr);
        return 6;
    }

    DebugPrint(L"[EH4] Installed ZwRaiseException hook\n");

    return 0;
}

int EH4_RemoveRaiseHook()
{
    DETOUR_TRANSACTION_OPTIONS Options;
    Options.fSuspendThreads = FALSE;

    HRESULT hr = SlimDetoursTransactionBeginEx(&Options);
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursTransactionBeginEx failed: %08X", hr);
        return 1;
    }

    hr = SlimDetoursDetach((PVOID*)&g_pfnZwRaiseException, EH4_ZwRaiseException_Hook);
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursDetach failed: %08X", hr);
        SlimDetoursTransactionAbort();
        return 2;
    }

    hr = SlimDetoursTransactionCommit();
    if (FAILED(hr))
    {
        DebugPrint(L"[EH4] SlimDetoursTransactionCommit failed: %08X", hr);
        return 3;
    }

    return 0;
}

LONG EH4_UnhandledExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    EH4_CRASHINFO CrashInfo;
    CrashInfo.pStoredExceptionInformation = ExceptionInfo;
    CrashInfo.ThreadId = GetCurrentThreadId();

    g_pStoredExceptionInformation = &CrashInfo;

    SetEvent(g_hCrashNotificationEvent);

    g_pfnNtSuspendProcess(g_hCurrentProcess);

    if (g_settings.callPreviousHandler)
    {
        return g_pfnPreviousFilter(ExceptionInfo);
    }
    else
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }
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
            L"eh4_watchdog.exe %p %p %p %p %i", 
            g_hCurrentProcess, 
            g_hCrashNotificationEventHandle, 
            g_hStartedEventHandle, 
            &g_pStoredExceptionInformation,
            g_settings.callPreviousHandler
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
    memcpy(&g_settings, Settings, sizeof(*Settings));

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
        L"eh4_watchdog.exe %p %p %p %p %i",
        g_hCurrentProcess, 
        g_hCrashNotificationEventHandle, 
        g_hStartedEventHandle, 
        &g_pStoredExceptionInformation,
        g_settings.callPreviousHandler
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

    g_uLastErrorMode = SetErrorMode(g_settings.sem);

    DebugPrint(L"[EH4] Waiting on watchdog to say hello");

    WaitForSingleObject(g_hStartedEvent, INFINITE);

    DebugPrint(L"[EH4] WaitForSingleObject resumed hStartedEvent");

    // EH4_InstallDispatcherHook();
    EH4_InstallRaiseHook();

    g_pfnPreviousFilter = SetUnhandledExceptionFilter(EH4_UnhandledExceptionHandler);

    return 0;
}

int EH4_Detach()
{
    g_bStopFlag = TRUE;

    SetUnhandledExceptionFilter(g_pfnPreviousFilter);
    EH4_RemoveRaiseHook();
    SetErrorMode(g_uLastErrorMode);

    TerminateProcess(g_hExceptionHandlerWatchdogProcess, 0);
    WaitForSingleObject(g_hWatchdogThread, INFINITE);
    CloseHandle(g_hExceptionHandlerWatchdogProcess);
    CloseHandle(g_hStartedEventHandle);
    CloseHandle(g_hCrashNotificationEventHandle);
    CloseHandle(g_hCurrentProcess);
    CloseHandle(g_hStartedEvent);
    CloseHandle(g_hCrashNotificationEvent);

    DebugPrint(L"[EH4] Detached");

    return 0;
}