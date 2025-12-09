#include <eh4_watchdog.h>
#include <eh4_ehdata.h>

#include <immintrin.h>

int EH4_DumpVectorRegisters(
    _In_ HANDLE hProcess,
    _In_ PCONTEXT pLocalContext,
    _In_ LPVOID pRemoteXSave,
    _In_ PXSAVE_AREA_HEADER pXSaveHeader,
    _In_ PXSTATE_CONFIGURATION pXStateConfig
);

int EH4_CalculateXStateOffsetCompacted(
    _In_  PXSTATE_CONFIGURATION pXStateConfig,
    _In_  DWORD64 CompactionMask,
    _In_  DWORD FeatureIndex,
    _Out_ PDWORD pOffset
)
{
    DWORD Offset = sizeof(XSAVE_AREA_HEADER);

    for (DWORD i = 2; i < FeatureIndex; ++i)
    {
        if (CompactionMask & (1ULL << i))
        {
            if (pXStateConfig->AlignedFeatures & (1ULL << i))
            {
                Offset = (Offset + 63) & ~63;
            }
            Offset += pXStateConfig->AllFeatures[i];
        }
    }

    if (pXStateConfig->AlignedFeatures & (1ULL << FeatureIndex))
    {
        Offset = (Offset + 63) & ~63;
    }

    *pOffset = Offset;
    return 0;
}

int EH4_ReadContextXState(
    _In_  HANDLE hProcess,
    _In_  LPVOID pRemoteContext,
    _In_  PCONTEXT pLocalContext,
    _Out_ PXSAVE_CET_U_FORMAT pCetState
)
{
    CONTEXT_EX RemoteContextEx;
    XSAVE_AREA_HEADER XSaveHeader;
    XSAVE_CET_U_FORMAT CetData;
    SIZE_T ReadBytesN;
    BOOL bSuccess;
    DWORD CetOffset;

    memset(pCetState, 0, sizeof(XSAVE_CET_U_FORMAT));

    if (!(pLocalContext->ContextFlags & CONTEXT_XSTATE))
    {
        DebugPrint(L"XState not present in context flags");
        return 1;
    }

    LPVOID pRemoteContextEx = (LPBYTE)pRemoteContext + sizeof(CONTEXT);
    bSuccess = ReadProcessMemory(hProcess, pRemoteContextEx, &RemoteContextEx, sizeof(CONTEXT_EX), &ReadBytesN);
    if (!bSuccess)
    {
        DebugPrint(L"Failed to read CONTEXT_EX");
        return 2;
    }

    if (RemoteContextEx.XState.Length == 0)
    {
        DebugPrint(L"XState length is 0");
        return 3;
    }

    LPVOID pRemoteXSave = (LPBYTE)pRemoteContextEx + RemoteContextEx.XState.Offset;
    bSuccess = ReadProcessMemory(hProcess, pRemoteXSave, &XSaveHeader, sizeof(XSAVE_AREA_HEADER), &ReadBytesN);
    if (!bSuccess)
    {
        DebugPrint(L"Failed to read XSAVE_AREA_HEADER");
        return 4;
    }

    DebugPrint(L"XSAVE_AREA_HEADER.Mask:\t%016llX", XSaveHeader.Mask);
    DebugPrint(L"XSAVE_AREA_HEADER.CompactionMask:\t%016llX", XSaveHeader.CompactionMask);

    if (!(XSaveHeader.Mask & XSTATE_MASK_CET_U))
    {
        DebugPrint(L"CET_U state not present in XSAVE mask");
        return 5;
    }

    PKUSER_SHARED_DATA pSharedUserData = (PKUSER_SHARED_DATA)0x7FFE0000;
    PXSTATE_CONFIGURATION pXStateConfig = &pSharedUserData->XState;

    if (XSaveHeader.CompactionMask & (1ULL << 63))
    {
        EH4_CalculateXStateOffsetCompacted(pXStateConfig, XSaveHeader.CompactionMask, XSTATE_CET_U, &CetOffset);
        DebugPrint(L"CET offset (compacted):\t%u", CetOffset);
    }
    else
    {
        CetOffset = pXStateConfig->Features[XSTATE_CET_U].Offset;
        DebugPrint(L"CET offset (non-compacted):\t%u", CetOffset);
    }

    LPVOID pRemoteCet = (LPBYTE)pRemoteXSave + CetOffset;
    bSuccess = ReadProcessMemory(hProcess, pRemoteCet, &CetData, sizeof(XSAVE_CET_U_FORMAT), &ReadBytesN);
    if (!bSuccess)
    {
        DebugPrint(L"Failed to read CET state");
        return 6;
    }

    *pCetState = CetData;
    return 0;
}

int EH4_DumpVectorRegisters(
    _In_ HANDLE hProcess,
    _In_ PCONTEXT pLocalContext,
    _In_ LPVOID pRemoteXSave,
    _In_ PXSAVE_AREA_HEADER pXSaveHeader,
    _In_ PXSTATE_CONFIGURATION pXStateConfig
)
{
    SIZE_T ReadBytesN;
    BOOL bSuccess;
    DWORD Offset;
    DWORD64 FeatureMask = pXStateConfig->EnabledFeatures;
    BOOL bCompacted = (pXSaveHeader->CompactionMask & (1ULL << 63)) != 0;

    if (FeatureMask & XSTATE_MASK_LEGACY_SSE)
    {
    }

    if (FeatureMask & XSTATE_MASK_AVX)
    {
        DWORD YmmHi[64];

        if (bCompacted)
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, pXSaveHeader->CompactionMask, XSTATE_AVX, &Offset);
        else
            Offset = pXStateConfig->Features[XSTATE_AVX].Offset;

        bSuccess = ReadProcessMemory(hProcess, (LPBYTE)pRemoteXSave + Offset, YmmHi, sizeof(YmmHi), &ReadBytesN);
    }

    if (FeatureMask & XSTATE_MASK_AVX512)
    {
        if (bCompacted)
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, pXSaveHeader->CompactionMask, XSTATE_AVX512_ZMM_H, &Offset);
        else
            Offset = pXStateConfig->Features[XSTATE_AVX512_ZMM_H].Offset;

        bSuccess = ReadProcessMemory(hProcess, (LPBYTE)pRemoteXSave + Offset, ZmmHi256, sizeof(ZmmHi256), &ReadBytesN);

        if (bCompacted)
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, pXSaveHeader->CompactionMask, XSTATE_AVX512_ZMM, &Offset);
        else
            Offset = pXStateConfig->Features[XSTATE_AVX512_ZMM].Offset;

        bSuccess = ReadProcessMemory(hProcess, (LPBYTE)pRemoteXSave + Offset, Zmm16_31, sizeof(Zmm16_31), &ReadBytesN);
    }

    return 0;
}
int EH4_DumpShadowStack(
    _In_ HANDLE hProcess,
    _In_ DWORD64 Ssp,
    _In_ DWORD64 Rsp,
    _In_ DWORD MaxEntries
)
{
    SIZE_T ReadBytesN;
    BOOL bSuccess;

    DebugPrint(L"[SSP] %016llX", Ssp);

    for (DWORD i = 0; i < MaxEntries; ++i)
    {
        DWORD64 ShadowEntry;
        LPVOID pAddr = (LPVOID)(Ssp + (i * sizeof(DWORD64)));
        
        bSuccess = ReadProcessMemory(hProcess, pAddr, &ShadowEntry, sizeof(DWORD64), &ReadBytesN);
        if (!bSuccess || ReadBytesN != sizeof(DWORD64))
        {
            DebugPrint(L"[+%02X] %016llX: <read failed>", i*8, (DWORD64)pAddr);
            break;
        }

        if (ShadowEntry == 0)
        {
            DebugPrint(L"[+%02X] %016llX: 0000000000000000 (end)", i*8, (DWORD64)pAddr);
            break;
        }

        DebugPrint(L"[+%02X] %016llX: %016llX", i*8, (DWORD64)pAddr, ShadowEntry);
    }

    DebugPrint(L"[RSP] %016llX", Rsp);

    for (DWORD i = 0; i < MaxEntries; ++i)
    {
        DWORD64 StackEntry;
        LPVOID pAddr = (LPVOID)(Rsp + (i * sizeof(DWORD64)));

        bSuccess = ReadProcessMemory(hProcess, pAddr, &StackEntry, sizeof(DWORD64), &ReadBytesN);
        if (!bSuccess || ReadBytesN != sizeof(DWORD64))
        {
            DebugPrint(L"[+%02X] %016llX: <read failed>", i*8, (DWORD64)pAddr);
            break;
        }

        DebugPrint(L"[+%02X] %016llX: %016llX", i*8, (DWORD64)pAddr, StackEntry);
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

    PEXCEPTION_POINTERS pRemoteExceptionPointersValue;
    EXCEPTION_POINTERS RemoteExceptionPointers;
    SIZE_T ReadBytesN;
    BOOL bSuccess;

    bSuccess = ReadProcessMemory(hProcess, pStoredExceptionInformation, &pRemoteExceptionPointersValue, sizeof(PEXCEPTION_POINTERS), &ReadBytesN);
    bSuccess = ReadProcessMemory(hProcess, pRemoteExceptionPointersValue, &RemoteExceptionPointers, sizeof(EXCEPTION_POINTERS), &ReadBytesN);
    bSuccess = ReadProcessMemory(hProcess, RemoteExceptionPointers.ExceptionRecord, &ExceptionRecord, sizeof(EXCEPTION_RECORD), &ReadBytesN);
    bSuccess = ReadProcessMemory(hProcess, RemoteExceptionPointers.ContextRecord, &ContextRecord, sizeof(CONTEXT), &ReadBytesN);

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

    PKUSER_SHARED_DATA pSharedUserData = (PKUSER_SHARED_DATA)0x7FFE0000;

    CONTEXT_EX RemoteContextEx;
    XSAVE_AREA_HEADER XSaveHeader;
    LPVOID pRemoteContextEx = (LPBYTE)RemoteExceptionPointers.ContextRecord + sizeof(CONTEXT);
    ReadProcessMemory(hProcess, pRemoteContextEx, &RemoteContextEx, sizeof(CONTEXT_EX), &ReadBytesN);
    LPVOID pRemoteXSave = (LPBYTE)pRemoteContextEx + RemoteContextEx.XState.Offset;
    ReadProcessMemory(hProcess, pRemoteXSave, &XSaveHeader, sizeof(XSAVE_AREA_HEADER), &ReadBytesN);

    EH4_DumpVectorRegisters(hProcess, &ContextRecord, pRemoteXSave, &XSaveHeader, &pSharedUserData->XState);

    PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy;
    GetProcessMitigationPolicy(hProcess, ProcessUserShadowStackPolicy, &policy, sizeof(policy));
    DebugPrint(L"EnableUserShadowStack:\t%u", policy.EnableUserShadowStack);

    DebugPrint(L"XState.EnabledFeatures:\t%016llX", pSharedUserData->XState.EnabledFeatures);

    XSAVE_CET_U_FORMAT CetState;
    if (!EH4_ReadContextXState(hProcess, RemoteExceptionPointers.ContextRecord, &ContextRecord, &CetState))
    {
        DebugPrint(L"CET MSR:\t%016llX", CetState.Ia32CetUMsr);
        DebugPrint(L"SSP:\t%016llX", CetState.Ia32Pl3SspMsr);
        
        DebugPrint(L"Ia32CetUMsr:\t%lu", CetState.Ia32CetUMsr);

        if ((CetState.Ia32CetUMsr & 1) && CetState.Ia32Pl3SspMsr != 0)
        {
            EH4_DumpShadowStack(hProcess, CetState.Ia32Pl3SspMsr, ContextRecord.Rsp, 32);
        }
    }

    EH4_ProcessCXXException(hProcess, &ExceptionPointers);

    return 0;
}