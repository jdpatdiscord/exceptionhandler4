#include <eh4_agent_processing.h>
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

int EH4_ProcessGNUCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
)
{
    return 0;
}

#define PAGE_CHUNK 0x1000

int EH4_ReadRemoteStringA(
    _In_  HANDLE hProcess,
    _In_  PCVOID pRemoteString,
    _Out_writes_z_(cchBuffer) LPSTR pBuffer,
    _In_  SIZE_T cchBuffer
)
{
    if (cchBuffer == 0)
    {
        return 1;
    }

    pBuffer[0] = '\0';

    if (!pRemoteString)
    {
        return 1;
    }

    SIZE_T offset = 0;
    SIZE_T maxLen = cchBuffer - 1;
    ULONG_PTR addr = (ULONG_PTR)pRemoteString;

    while (offset < maxLen)
    {
        SIZE_T toPageEnd = ((addr + PAGE_CHUNK) & ~(PAGE_CHUNK - 1)) - addr;
        SIZE_T toRead = min(toPageEnd, maxLen - offset);
        SIZE_T nRead;

        if (!ReadProcessMemory(hProcess, (PVOID)addr, pBuffer + offset, toRead, &nRead))
        {
            pBuffer[offset] = '\0';
            return (offset > 0) ? 0 : 2;
        }

        char* null = memchr(pBuffer + offset, '\0', nRead);
        if (null)
        {
            return 0;
        }

        offset += nRead;
        addr += nRead;
    }

    pBuffer[maxLen] = '\0';
    return 0;
}

int EH4_ProcessMSCCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
)
{
    SIZE_T ReadBytesN;

    if (pExceptionPointers->ExceptionRecord->NumberParameters < 3)
    {
        DebugPrint(L"NumberParameters too low");
        return 1;
    }

    DWORD MagicNumber       = (DWORD)pExceptionPointers->ExceptionRecord->ExceptionInformation[0];
    PVOID pExceptionObject  = (PVOID)pExceptionPointers->ExceptionRecord->ExceptionInformation[1];
    PVOID pThrowInfo        = (PVOID)pExceptionPointers->ExceptionRecord->ExceptionInformation[2];
    PVOID pThrowImageBase   = NULL;

    if (pExceptionPointers->ExceptionRecord->NumberParameters >= 4)
    {
        pThrowImageBase = (PVOID)pExceptionPointers->ExceptionRecord->ExceptionInformation[3];
    }

    DebugPrint(L"CXX Exception Record");
    DebugPrint(L"\tMagicNumber:      %08X", MagicNumber);
    DebugPrint(L"\tpExceptionObject: %016llX", (ULONG64)pExceptionObject);
    DebugPrint(L"\tpThrowInfo:       %016llX", (ULONG64)pThrowInfo);
    DebugPrint(L"\tpThrowImageBase:  %016llX", (ULONG64)pThrowImageBase);

    if (MagicNumber != EH_MAGIC_NUMBER1 &&
        MagicNumber != EH_MAGIC_NUMBER2 &&
        MagicNumber != EH_MAGIC_NUMBER3)
    {
        DebugPrint(L"Unknown CXX magic number (!!!)");
        return 2;
    }

    // pThrowInfo can be NULL for rethrow (throw;)
    if (pThrowInfo == NULL)
    {
        DebugPrint(L"rethrow");
        return 0;
    }

    char ExceptionWhat[512];
    strcpy_s(ExceptionWhat, sizeof(ExceptionWhat), "[EH4] Unknown exception!");

    EH_STL_EXCEPTION STLExceptionRemoteCopy;
    if (pExceptionObject && !ReadProcessMemory(hProcess, pExceptionObject, &STLExceptionRemoteCopy, sizeof(STLExceptionRemoteCopy), &ReadBytesN))
    {
        DebugPrint(L"Couldn't read external process for EH_STL_EXCEPTION");
        return 3;
    }

    if (STLExceptionRemoteCopy.what && EH4_ReadRemoteStringA(hProcess, STLExceptionRemoteCopy.what, ExceptionWhat, sizeof(ExceptionWhat)))
    {
        DebugPrint(L"Couldn't read external process for ExceptionWhat");
        return 4;
    }

    if (STLExceptionRemoteCopy.what)
    {
        DebugPrint(L"\t\tpExceptionObject -> .what(): %S", ExceptionWhat);
    }

    EH_THROWINFO_RELATIVE ThrowInfo;
    if (!ReadProcessMemory(hProcess, pThrowInfo, &ThrowInfo, sizeof(ThrowInfo), &ReadBytesN))
    {
        DebugPrint(L"Couldn't read external process for EH_THROWINFO_RELATIVE");
        return 5;
    }

    DebugPrint(L"\tThrowInfo.attributes:           %08X", ThrowInfo.attributes);
    DebugPrint(L"\tThrowInfo.pmfnUnwind:           %08X (RVA)", ThrowInfo.pmfnUnwind);
    DebugPrint(L"\tThrowInfo.pForwardCompat:       %08X (RVA)", ThrowInfo.pForwardCompat);
    DebugPrint(L"\tThrowInfo.pCatchableTypeArray:  %08X (RVA)", ThrowInfo.pCatchableTypeArray);

    if (!ThrowInfo.pCatchableTypeArray || !pThrowImageBase)
    {
        return 0;
    }
    
    PVOID pCatchableTypeArray = (PVOID)((ULONG_PTR)pThrowImageBase + ThrowInfo.pCatchableTypeArray);

    EH_CATCHABLETYPEARRAY_RELATIVE CatchableTypeArray;
    if (!ReadProcessMemory(hProcess, pCatchableTypeArray, &CatchableTypeArray, sizeof(int), &ReadBytesN))
    {
        DebugPrint(L"Couldn't read external process for EH_CATCHABLETYPEARRAY_RELATIVE");
        return 6;
    }

    DebugPrint(L"\tCatchableTypeArray.nCatchableTypes: %i", CatchableTypeArray.nCatchableTypes);

    int* TypeRVAs = (int*)malloc(CatchableTypeArray.nCatchableTypes * sizeof(int));
    if (!TypeRVAs)
    {
        DebugPrint(L"Couldn't malloc for CatchableTypeArray.nCatchableTypes");
        return 7;
    }

    PVOID pArrayStart = (PVOID)((ULONG_PTR)pCatchableTypeArray + sizeof(int));
    if (!ReadProcessMemory(
            hProcess,
            pArrayStart,
            TypeRVAs, 
            CatchableTypeArray.nCatchableTypes * sizeof(int),
            &ReadBytesN))
    {
        free(TypeRVAs);
        return 8;
    }

    for (int i = 0; i < CatchableTypeArray.nCatchableTypes; ++i)
    {
        PVOID pCatchableType = (PVOID)((ULONG_PTR)pThrowImageBase + TypeRVAs[i]);

        EH_CATCHABLETYPE_RELATIVE CatchableType;
        if (!ReadProcessMemory(hProcess, pCatchableType, &CatchableType, sizeof(CatchableType), &ReadBytesN))
        {
            DebugPrint(L"Couldn't read external process for EH_CATCHABLETYPE_RELATIVE: i = %i", i);
            continue;
        }

        PVOID pTypeDescriptor = (PVOID)((ULONG_PTR)pThrowImageBase + CatchableType.pType);

        char TypeName[8192];
        memset(TypeName, 0, sizeof(TypeName));

        PVOID pName = (PVOID)((ULONG_PTR)pTypeDescriptor + offsetof(EH_TYPEDESCRIPTOR64, name));
        ReadProcessMemory(hProcess, pName, TypeName, sizeof(TypeName) - 1, &ReadBytesN);
        DebugPrint(L"\tCatchableType[%d]:", i);
        DebugPrint(L"\t\tproperties:   %08X", CatchableType.properties);
        DebugPrint(L"\t\tsizeOrOffset: %i", CatchableType.sizeOrOffset);
        DebugPrint(L"\t\tname:         %S", TypeName);
    }

    free(TypeRVAs);

    return 0;
}

int EH4_ProcessCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
)
{
    DWORD ExceptionCode = pExceptionPointers->ExceptionRecord->ExceptionCode;
    if (ExceptionCode != '\xe0msc' && ExceptionCode != ' GCC') 
    {
        DebugPrint(L"Not a CXX exception");
        return 0;
    }

    if (ExceptionCode == '\xe0msc')
    {
        DebugPrint(L"Microsoft ABI exception");
        return EH4_ProcessMSCCXXException(hProcess, pExceptionPointers);
    }

    if (ExceptionCode == ' GCC')
    {
        DebugPrint(L"GNU libstd++ ABI exception");
        return EH4_ProcessGNUCXXException(hProcess, pExceptionPointers);
    }

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