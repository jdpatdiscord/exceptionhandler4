#include <eh4_watchdog.h>
#include <eh4_ehdata.h>

int EH4_ReadExceptionPointers(
    _In_  HANDLE hProcess, 
    _Out_ PEXCEPTION_POINTERS pLocalExceptionPointers,
    _In_  LPVOID pRemoteExceptionPointers
)
{
    PEXCEPTION_POINTERS pRemoteExceptionPointersValue;
    EXCEPTION_POINTERS RemoteExceptionPointers;
    SIZE_T ReadBytesN;
    BOOL bSuccess;
    bSuccess = ReadProcessMemory(hProcess, pRemoteExceptionPointers, &pRemoteExceptionPointersValue, sizeof(PEXCEPTION_POINTERS), &ReadBytesN);
    bSuccess = ReadProcessMemory(hProcess, pRemoteExceptionPointersValue, &RemoteExceptionPointers, sizeof(EXCEPTION_POINTERS), &ReadBytesN);
    bSuccess = ReadProcessMemory(hProcess, RemoteExceptionPointers.ExceptionRecord, pLocalExceptionPointers->ExceptionRecord, sizeof(EXCEPTION_RECORD), &ReadBytesN);
    bSuccess = ReadProcessMemory(hProcess, RemoteExceptionPointers.ContextRecord, pLocalExceptionPointers->ContextRecord, sizeof(CONTEXT), &ReadBytesN);
    return 0;
}

int EH4_ProcessException(
    _In_  HANDLE hProcess,
    _Out_ LPVOID pStoredExceptionInformation
)
{
    EXCEPTION_RECORD ExceptionRecord;
    memset(&ExceptionRecord, 0, sizeof(ExceptionRecord));

    CONTEXT ContextRecord;
    memset(&ContextRecord, 0, sizeof(ContextRecord));

    EXCEPTION_POINTERS ExceptionPointers;
    memset(&ExceptionPointers, 0, sizeof(ExceptionPointers));

    ExceptionPointers.ExceptionRecord = &ExceptionRecord;
    ExceptionPointers.ContextRecord = &ContextRecord;

    EH4_ReadExceptionPointers(
            hProcess,
            &ExceptionPointers,
            pStoredExceptionInformation
            );

    DebugPrint(L"Excep code:\t%08X", ExceptionRecord.ExceptionCode);
    DebugPrint(L"Excep flags:\t%08X", ExceptionRecord.ExceptionFlags);
    DebugPrint(L"Excep params:\t%08X", ExceptionRecord.NumberParameters);
    for (DWORD i = 0; i < ExceptionRecord.NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
    {
        DebugPrint(L"\tParam[%u]:\t%016llX", i, ExceptionRecord.ExceptionInformation[i]);
    }

    DebugPrint(L"Rax\t%016llX", ContextRecord.Rax);
    DebugPrint(L"Rbx\t%016llX", ContextRecord.Rbx);
    DebugPrint(L"Rcx\t%016llX", ContextRecord.Rcx);
    DebugPrint(L"Rdx\t%016llX", ContextRecord.Rdx);
    DebugPrint(L"Rsi\t%016llX", ContextRecord.Rsi);
    DebugPrint(L"Rdi\t%016llX", ContextRecord.Rdi);
    DebugPrint(L"Rbp\t%016llX", ContextRecord.Rbp);
    DebugPrint(L"Rsp\t%016llX", ContextRecord.Rsp);
    DebugPrint(L"R8\t%016llX",  ContextRecord.R8);
    DebugPrint(L"R9\t%016llX",  ContextRecord.R9);
    DebugPrint(L"R10\t%016llX", ContextRecord.R10);
    DebugPrint(L"R11\t%016llX", ContextRecord.R11);
    DebugPrint(L"R12\t%016llX", ContextRecord.R12);
    DebugPrint(L"R13\t%016llX", ContextRecord.R13);
    DebugPrint(L"R14\t%016llX", ContextRecord.R14);
    DebugPrint(L"R15\t%016llX", ContextRecord.R15);
    DebugPrint(L"Rip\t%016llX", ContextRecord.Rip);

    DebugPrint(L"CS\t%i", ContextRecord.SegCs);
    DebugPrint(L"DS\t%i", ContextRecord.SegDs);
    DebugPrint(L"ES\t%i", ContextRecord.SegEs);
    DebugPrint(L"FS\t%i", ContextRecord.SegFs);
    DebugPrint(L"GS\t%i", ContextRecord.SegGs);
    DebugPrint(L"SS\t%i", ContextRecord.SegSs);

    EH4_ProcessCXXException(hProcess, &ExceptionPointers);

    return 0;
}