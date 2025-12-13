#include <eh4_watchdog.h>
#include <eh4_registertable.h>

int EH4_CalculateXStateOffsetCompacted(
    _In_  PXSTATE_CONFIGURATION pXStateConfig,
    _In_  DWORD64 CompactionMask,
    _In_  DWORD FeatureIndex,
    _Out_ PDWORD pOffset
)
{
    if (FeatureIndex < 2)
    {
        DebugPrint(L"XSTATE does not contain values from FXSAVE");
        return 1; 
    }

    if (!(CompactionMask & (1ULL << FeatureIndex)))
    {
        DebugPrint(L"Requested feature does not exist in compaction mask");
        return 2;
    }

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

int EH4_ProcessXStateCET(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers,
    _In_ PCONTEXT pRemoteContext
)
{
    SIZE_T ReadBytesN;
    BOOL bSuccess;

    PKUSER_SHARED_DATA pSharedUserData = (PKUSER_SHARED_DATA)0x7FFE0000;
    PXSTATE_CONFIGURATION pXStateConfig = &pSharedUserData->XState;

    CONTEXT_EX RemoteContextEx;
    XSAVE_AREA_HEADER XSaveHeader;
    LPVOID pRemoteContextEx = (PCHAR)pRemoteContext + sizeof(CONTEXT);
    bSuccess = ReadProcessMemory(hProcess, pRemoteContextEx, &RemoteContextEx, sizeof(CONTEXT_EX), &ReadBytesN);
    LPVOID pRemoteXSave = (PCHAR)pRemoteContextEx + RemoteContextEx.XState.Offset;
    bSuccess = ReadProcessMemory(hProcess, pRemoteXSave, &XSaveHeader, sizeof(XSAVE_AREA_HEADER), &ReadBytesN);

    XSAVE_CET_U_FORMAT CETData;
    DWORD OffsetCET;

    if (!(pExceptionPointers->ContextRecord->ContextFlags & CONTEXT_XSTATE))
    {
        DebugPrint(L"CONTEXT_XSTATE is not enabled in ContextRecord");
        return 0;
    }

    if (RemoteContextEx.XState.Length == 0)
    {
        DebugPrint(L"XState has no length");
        return 0;
    }

    if (!(XSaveHeader.Mask & XSTATE_MASK_CET_U))
    {
        DebugPrint(L"XSTATE_MASK_CET_U is not enabled");
        return 0;
    }

    DebugPrint(L"CET & shadow stack are enabled");

    if (XSaveHeader.CompactionMask & (1ULL << 63))
    {
        EH4_CalculateXStateOffsetCompacted(pXStateConfig, XSaveHeader.CompactionMask, XSTATE_CET_U, &OffsetCET);
    }
    else
    {
        OffsetCET = pXStateConfig->Features[XSTATE_CET_U].Offset;
    }

    bSuccess = ReadProcessMemory(hProcess, (PCHAR)pRemoteXSave + OffsetCET, &CETData, sizeof(XSAVE_CET_U_FORMAT), &ReadBytesN);
    if (!bSuccess)
    {
        DebugPrint(L"Couldn't read external XSAVE_CET_U_FORMAT");
        return 1;
    }

    // Table 2-2. IA-32 Architectural MSRs, "Configure User Mode CET (R/W)".
    // Intel® 64 and IE-21 Architectures Software Developer's Manual, Combined Volumes: 1, 2, 3, and 4

    if (!(CETData.Ia32CetUMsr & 1))
    {
        DebugPrint(L"SH_STK_EN flag in MSR is not set");
        return 0;
    }

    DebugPrint(L"Ssp:\t%016llX", CETData.Ia32Pl3SspMsr);

    return 0;
}

int EH4_ProcessXStateFP(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers,
    _In_ PCONTEXT pRemoteContext
)
{
    SIZE_T ReadBytesN;

    CONTEXT_EX RemoteContextEx;
    XSAVE_AREA_HEADER XSaveHeader;
    LPVOID pRemoteContextEx = (PCHAR)pRemoteContext + sizeof(CONTEXT);
    ReadProcessMemory(hProcess, pRemoteContextEx, &RemoteContextEx, sizeof(CONTEXT_EX), &ReadBytesN);
    LPVOID pRemoteXSave = (PCHAR)pRemoteContextEx + RemoteContextEx.XState.Offset;
    ReadProcessMemory(hProcess, pRemoteXSave, &XSaveHeader, sizeof(XSAVE_AREA_HEADER), &ReadBytesN);

    PKUSER_SHARED_DATA pSharedUserData = (PKUSER_SHARED_DATA)0x7FFE0000;
    PXSTATE_CONFIGURATION pXStateConfig = &pSharedUserData->XState;

    DWORD64 FeatureMask = pXStateConfig->EnabledFeatures;
    BOOL bCompacted = (XSaveHeader.CompactionMask & (1ULL << 63)) != 0;

    DWORD OffsetSSE_XMM0_XMM15 = 0;
    DWORD OffsetAVX_YMM0HI_YMM15HI = 0;
    DWORD OffsetAVX512_ZMM0HI_ZMM15HI = 0;
    DWORD OffsetAVX512_ZMM16_ZMM31 = 0;
    DWORD OffsetAMX_DATA = 0;

    if (bCompacted)
    {
        if (FeatureMask & XSTATE_MASK_AVX)
        {
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, XSaveHeader.CompactionMask, XSTATE_AVX, &OffsetAVX_YMM0HI_YMM15HI);
        }

        if (FeatureMask & XSTATE_MASK_AVX512)
        {
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, XSaveHeader.CompactionMask, XSTATE_AVX512_ZMM_H, &OffsetAVX512_ZMM0HI_ZMM15HI);
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, XSaveHeader.CompactionMask, XSTATE_AVX512_ZMM, &OffsetAVX512_ZMM16_ZMM31);
        }

        if (FeatureMask & XSTATE_MASK_AMX_TILE_DATA)
        {
            EH4_CalculateXStateOffsetCompacted(pXStateConfig, XSaveHeader.CompactionMask, XSTATE_AMX_TILE_DATA, &OffsetAMX_DATA);
        }
    }
    else
    {
        OffsetAVX_YMM0HI_YMM15HI = pXStateConfig->Features[XSTATE_AVX].Offset;
        OffsetAVX512_ZMM0HI_ZMM15HI = pXStateConfig->Features[XSTATE_AVX512_ZMM_H].Offset;
        OffsetAVX512_ZMM16_ZMM31 = pXStateConfig->Features[XSTATE_AVX512_ZMM].Offset;
        OffsetAMX_DATA = pXStateConfig->Features[XSTATE_AMX_TILE_DATA].Offset;
    }

    if (FeatureMask & XSTATE_MASK_LEGACY_SSE)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            memcpy(g_fpregistertable_x64 + 0 + (i * NUM_ZMM_ELEMS), &pExceptionPointers->ContextRecord->FltSave.XmmRegisters[i], 16);
        }
    }
    if (FeatureMask & XSTATE_MASK_AVX)
    {
        float YMM[64];
        ReadProcessMemory(hProcess, (PCHAR)pRemoteXSave + OffsetAVX_YMM0HI_YMM15HI, YMM, sizeof(YMM), &ReadBytesN);
        for (unsigned i = 0; i < 16; ++i)
        {
            memcpy(g_fpregistertable_x64 + 4 + (i * NUM_ZMM_ELEMS), &YMM[i * 4], 16);
        }
    }
    if (FeatureMask & XSTATE_MASK_AVX512)
    {
        float ZMM0HI_ZMM15HI[128];
        ReadProcessMemory(hProcess, (PCHAR)pRemoteXSave + OffsetAVX512_ZMM0HI_ZMM15HI, ZMM0HI_ZMM15HI, sizeof(ZMM0HI_ZMM15HI), &ReadBytesN);
        for (unsigned i = 0; i < 16; ++i)
        {
            memcpy(g_fpregistertable_x64 + 8 + (i * NUM_ZMM_ELEMS), &ZMM0HI_ZMM15HI[i * 8], 32);
        }

        ReadProcessMemory(hProcess, (PCHAR)pRemoteXSave + OffsetAVX512_ZMM16_ZMM31, g_fpregistertable_x64 + 256, sizeof(float) * 256, &ReadBytesN);
    }
    if (FeatureMask & XSTATE_MASK_AMX_TILE_DATA)
    {
        ReadProcessMemory(hProcess, (PCHAR)pRemoteXSave + OffsetAMX_DATA, g_fpmatrixtiledata_x64, sizeof(g_fpmatrixtiledata_x64), &ReadBytesN);
    }

    //

    if (FeatureMask & XSTATE_MASK_LEGACY_SSE)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            UINT32* xmm_i = (UINT32*)g_fpregistertable_x64 + 0 + (i * NUM_ZMM_ELEMS);
            DebugPrint(L"xmm%i:\t%08X %08X %08X %08X", i, xmm_i[0], xmm_i[1], xmm_i[2], xmm_i[3]);
        }
    }

    if (FeatureMask & XSTATE_MASK_AVX)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            UINT32* ymm_i = (UINT32*)g_fpregistertable_x64 + 0 + (i * NUM_ZMM_ELEMS);
            DebugPrint(L"ymm%i:\t%08X %08X %08X %08X %08X %08X %08X %08X", i, ymm_i[0], ymm_i[1], ymm_i[2], ymm_i[3], ymm_i[4], ymm_i[5], ymm_i[6], ymm_i[7]);
        }
    }

    if (FeatureMask & XSTATE_MASK_AVX512)
    {
        for (unsigned i = 0; i < 32; ++i)
        {
            UINT32* zmm_i = (UINT32*)g_fpregistertable_x64 + 0 + (i * NUM_ZMM_ELEMS);
            DebugPrint(L"zmm%i:\t%08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X", i, 
                zmm_i[0], zmm_i[1], zmm_i[2], zmm_i[3], zmm_i[4], zmm_i[5], zmm_i[6], zmm_i[7],
                zmm_i[8], zmm_i[9], zmm_i[10], zmm_i[11], zmm_i[12], zmm_i[13], zmm_i[14], zmm_i[15]
                );
        }
    }

    return 0;
}