#include <phnt.h>

#include <eh4_agent_processing.h>

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

int main(int argc, char* argv[])
{
    DebugPrint(L"[Watchdog] External process started");
    if (argc < 5)
    {
        DebugPrint(L"[Watchdog] Not enough arguments, exiting now");
        return 1;
    }

    const char* processHandleStr = argv[1];
    const char* crashEventHandleStr = argv[2];
    const char* startEventHandleStr = argv[3];
    const char* exceptionPtrStr = argv[4];

    char* endParse;

    HANDLE hClientProcess = (HANDLE)strtoull(processHandleStr, &endParse, 16);
    HANDLE hCrashEvent = (HANDLE)strtoull(crashEventHandleStr, &endParse, 16);
    HANDLE hStartEvent = (HANDLE)strtoull(startEventHandleStr, &endParse, 16);
    LPVOID pStoredExceptionInformation = (LPVOID)strtoull(exceptionPtrStr, &endParse, 16);

    SetEvent(hStartEvent);

    WaitForSingleObject(hCrashEvent, INFINITE);

    DebugPrint(L"[Watchdog] Received crash notification");

    EH4_ProcessException(hClientProcess, pStoredExceptionInformation);

    DebugPrint(L"[Watchdog] Terminating client process");

    TerminateProcess(hClientProcess, 1);

    DebugPrint(L"[Watchdog] Watchdog lifecycle is finished, exiting now");

    return 0;
}