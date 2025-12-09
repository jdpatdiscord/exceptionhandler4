#include <eh4_watchdog.h>
#include <eh4_ehdata.h>

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

int EH4_ProcessGNUCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
)
{
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