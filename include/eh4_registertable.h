#pragma once

#ifndef EH4_REGISTERTABLE_H
#define EH4_REGISTERTABLE_H

#include <phnt.h>

#define NUM_XMM_ELEMS 4
#define NUM_YMM_ELEMS 8
#define NUM_ZMM_ELEMS 16

typedef struct
{
    char _bytes[10];
} EH4_X87REGISTER, *PEH4_X87REGISTER;

typedef struct _EH4_FPDISPLAYREGISTER
{
    union
    {
        PFLOAT RegisterDataFP32;
        PDOUBLE RegisterDataFP64;
    };
    UCHAR RegisterSize;
    PCSTR RegisterName;
} EH4_FPDISPLAYREGISTER, *PEH4_FPDISPLAYREGISTER;

typedef struct _EH4_GPDISPLAYREGISTER32
{
    PUINT32 RegisterData;
    PCSTR RegisterName;
} EH4_GPDISPLAYREGISTER32, *PEH4_GPDISPLAYREGISTER32;

typedef struct _EH4_GPDISPLAYREGISTER64
{
    PUINT64 RegisterData;
    PCSTR RegisterName;
} EH4_GPDISPLAYREGISTER64, *PEH4_GPDISPLAYREGISTER64;

// AMD64 CONTEXT: rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15, rip
EXTERN_C EH4_X87REGISTER g_x87registertable_x64[8];
EXTERN_C UINT64 g_gpregistertable_x64[17];
EXTERN_C FLOAT g_fpregistertable_x64[512]; // zmm0 - zmm31 (includes xmm, ymm)
EXTERN_C FLOAT g_fpmatrixtiledata_x64[2048]; // tmm0 - tmm7

// I386 CONTEXT: eax, ecx, edx, ebx, esp, ebp, esi, edi, eip
EXTERN_C EH4_X87REGISTER g_x87registertable_x86[8];
EXTERN_C UINT32 g_gpregistertable_x86[9];
EXTERN_C FLOAT g_fpregistertable_x86[32]; // xmm0 - xmm7

// ARM64 CONTEXT: X0 .. X30, SP, PC
EXTERN_C UINT64 g_gpregistertable_a64[33];
EXTERN_C FLOAT g_fpregistertable_a64[128];

// ARM CONTEXT: R0 .. R15
EXTERN_C UINT32 g_gpregistertable_a32[16];
// 32-bit NEON: only 16 128-bit registers vs. 32 on ARM64
EXTERN_C FLOAT g_fpregistertable_a32[64];

EXTERN_C EH4_GPDISPLAYREGISTER64 g_gpdisplayregisters_x64[];
EXTERN_C EH4_GPDISPLAYREGISTER32 g_gpdisplayregisters_x86[];
EXTERN_C EH4_GPDISPLAYREGISTER64 g_gpdisplayregisters_a64[];
EXTERN_C EH4_GPDISPLAYREGISTER32 g_gpdisplayregisters_a32[];

EXTERN_C EH4_FPDISPLAYREGISTER g_fpdisplayregisters_x64[];
EXTERN_C EH4_FPDISPLAYREGISTER g_fpdisplayregisters_x86[];
EXTERN_C EH4_FPDISPLAYREGISTER g_fpdisplayregisters_a64[];
EXTERN_C EH4_FPDISPLAYREGISTER g_fpdisplayregisters_a32[];

#endif // EH4_REGISTERTABLE_H