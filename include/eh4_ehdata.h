#pragma once

#ifndef EH4_EHDATA_H
#define EH4_EHDATA_H

#include <phnt.h>

#include <stdint.h>

#define EH_MAGIC_NUMBER1        0x19930520
#define EH_MAGIC_NUMBER2        0x19930521
#define EH_MAGIC_NUMBER3        0x19930522

// Pure managed / CLR: _EH_RELATIVE_FUNCINFO = 0, _EH_RELATIVE_TYPEINFO = 0, _RTTI_RELATIVE_TYPEINFO = 0
// x86 WoW64 on ARM64: _EH_RELATIVE_FUNCINFO = 1, _EH_RELATIVE_TYPEINFO = 0, _RTTI_RELATIVE_TYPEINFO = 0
// ARM32             : _EH_RELATIVE_FUNCINFO = 1, _EH_RELATIVE_TYPEINFO = 1, _RTTI_RELATIVE_TYPEINFO = 0
// x64, ARM64        : _EH_RELATIVE_FUNCINFO = 1, _EH_RELATIVE_TYPEINFO = 1, _RTTI_RELATIVE_TYPEINFO = 1

#define HT_IsConst             0x00000001 // type referenced is 'const' qualified
#define HT_IsVolatile          0x00000002 // type referenced is 'volatile' qualified
#define HT_IsUnaligned         0x00000004 // type referenced is 'unaligned' qualified
#define HT_IsReference         0x00000008 // catch type is by reference
#define HT_IsResumable         0x00000010 // the catch may choose to resume (Reserved)
#define HT_IsStdDotDot         0x00000040 // the catch is std C++ catch(...) which is supposed to catch only C++ exceptions
#define HT_IsBadAllocCompat    0x00000080 // the WinRT type can catch a std::bad_alloc
#define HT_IsComplusEh         0x80000000 // Is handling within complus EH

#define HT_ISCONST(ht)			(HT_ADJECTIVES(ht) & HT_IsConst)		// Is the type referenced 'const' qualified
#define HT_ISVOLATILE(ht)		(HT_ADJECTIVES(ht) & HT_IsVolatile)		// Is the type referenced 'volatile' qualified
#define HT_ISUNALIGNED(ht)		(HT_ADJECTIVES(ht) & HT_IsUnaligned)	// Is the type referenced 'unaligned' qualified
#define HT_ISREFERENCE(ht)		(HT_ADJECTIVES(ht) & HT_IsReference)	// Is the catch type by reference
#define HT_ISRESUMABLE(ht)		(HT_ADJECTIVES(ht) & HT_IsResumable)	// Might the catch choose to resume (Reserved)
#define HT_ISCOMPLUSEH(ht)      (HT_ADJECTIVES(ht) & HT_IsComplusEh)
#define HT_ISBADALLOCCOMPAT(ht) (HT_ADJECTIVES(ht) & HT_IsBadAllocCompat)
#define HT_IS_STD_DOTDOT(ht)    (HT_ADJECTIVES(ht) & HT_IsStdDotDot)

#pragma pack(push, 4)

//
// Forward declarations
//
typedef struct _EH_TYPEDESCRIPTOR64 EH_TYPEDESCRIPTOR64, *PEH_TYPEDESCRIPTOR64;
typedef struct _EH_TYPEDESCRIPTOR32 EH_TYPEDESCRIPTOR32, *PEH_TYPEDESCRIPTOR32;
typedef struct _EH_CATCHABLETYPE_RELATIVE EH_CATCHABLETYPE_RELATIVE, *PEH_CATCHABLETYPE_RELATIVE;
typedef struct _EH_CATCHABLETYPE_ABSOLUTE64 EH_CATCHABLETYPE_ABSOLUTE64, *PEH_CATCHABLETYPE_ABSOLUTE64;
typedef struct _EH_CATCHABLETYPE_ABSOLUTE32 EH_CATCHABLETYPE_ABSOLUTE32, *PEH_CATCHABLETYPE_ABSOLUTE32;
typedef struct _EH_CATCHABLETYPEARRAY_RELATIVE EH_CATCHABLETYPEARRAY_RELATIVE, *PEH_CATCHABLETYPEARRAY_RELATIVE;
typedef struct _EH_CATCHABLETYPEARRAY_ABSOLUTE64 EH_CATCHABLETYPEARRAY_ABSOLUTE64, *PEH_CATCHABLETYPEARRAY_ABSOLUTE64;
typedef struct _EH_CATCHABLETYPEARRAY_ABSOLUTE32 EH_CATCHABLETYPEARRAY_ABSOLUTE32, *PEH_CATCHABLETYPEARRAY_ABSOLUTE32;
typedef struct _EH_THROWINFO_RELATIVE EH_THROWINFO_RELATIVE, *PEH_THROWINFO_RELATIVE;
typedef struct _EH_THROWINFO_ABSOLUTE64 EH_THROWINFO_ABSOLUTE64, *PEH_THROWINFO_ABSOLUTE64;
typedef struct _EH_THROWINFO_ABSOLUTE32 EH_THROWINFO_ABSOLUTE32, *PEH_THROWINFO_ABSOLUTE32;
typedef struct _EH_HANDLERTYPE_RELATIVE64 EH_HANDLERTYPE_RELATIVE64, *PEH_HANDLERTYPE_RELATIVE64;
typedef struct _EH_HANDLERTYPE_RELATIVE32 EH_HANDLERTYPE_RELATIVE32, *PEH_HANDLERTYPE_RELATIVE32;
typedef struct _EH_HANDLERTYPE_ABSOLUTE32 EH_HANDLERTYPE_ABSOLUTE32, *PEH_HANDLERTYPE_ABSOLUTE32;

typedef struct _EH_PMD
{
    int mdisp;
    int pdisp;
    int vdisp;
} EH_PMD, *PEH_PMD;


#pragma pack(push, 8)
// X64, ARM64
typedef struct _EH_TYPEDESCRIPTOR64
{
    PVOID __ptr64 pVFTable;
    PVOID __ptr64 spare;
    char name[1];
} EH_TYPEDESCRIPTOR64, *PEH_TYPEDESCRIPTOR64;
#pragma pack(pop)

// X86, ARM32, CHPE
typedef struct _EH_TYPEDESCRIPTOR32
{
    PVOID __ptr32 pVFTable;
    PVOID __ptr32 spare;
    char name[1];
} EH_TYPEDESCRIPTOR32, *PEH_TYPEDESCRIPTOR32;

// X64, ARM64, ARM32
typedef struct _EH_CATCHABLETYPE_RELATIVE
{
    DWORD properties;
    int pType;                                      // RVA to TypeDescriptor
    EH_PMD thisDisplacement;
    int sizeOrOffset;
    int copyFunction;                               // RVA
} EH_CATCHABLETYPE_RELATIVE, *PEH_CATCHABLETYPE_RELATIVE;

typedef struct _EH_CATCHABLETYPE_ABSOLUTE64
{
    DWORD properties;
    PEH_TYPEDESCRIPTOR64 __ptr64 pType;
    EH_PMD thisDisplacement;
    int sizeOrOffset;
    PVOID __ptr64 copyFunction;
} EH_CATCHABLETYPE_ABSOLUTE64, *PEH_CATCHABLETYPE_ABSOLUTE64;

// X86, CHPE
typedef struct _EH_CATCHABLETYPE_ABSOLUTE32
{
    DWORD properties;
    PEH_TYPEDESCRIPTOR32 __ptr32 pType;
    EH_PMD thisDisplacement;
    int sizeOrOffset;
    PVOID __ptr32 copyFunction;
} EH_CATCHABLETYPE_ABSOLUTE32, *PEH_CATCHABLETYPE_ABSOLUTE32;

// X64, ARM64, ARM32
typedef struct _EH_CATCHABLETYPEARRAY_RELATIVE
{
    int nCatchableTypes;
    int arrayOfCatchableTypes[1];                   // RVAs
} EH_CATCHABLETYPEARRAY_RELATIVE, *PEH_CATCHABLETYPEARRAY_RELATIVE;

typedef struct _EH_CATCHABLETYPEARRAY_ABSOLUTE64
{
    int nCatchableTypes;
    PEH_CATCHABLETYPE_ABSOLUTE64 __ptr64 arrayOfCatchableTypes[1];
} EH_CATCHABLETYPEARRAY_ABSOLUTE64, *PEH_CATCHABLETYPEARRAY_ABSOLUTE64;

// X86, CHPE
typedef struct _EH_CATCHABLETYPEARRAY_ABSOLUTE32
{
    int nCatchableTypes;
    PEH_CATCHABLETYPE_ABSOLUTE32 __ptr32 arrayOfCatchableTypes[1];
} EH_CATCHABLETYPEARRAY_ABSOLUTE32, *PEH_CATCHABLETYPEARRAY_ABSOLUTE32;

// X64, ARM64, ARM32
typedef struct _EH_THROWINFO_RELATIVE
{
    DWORD attributes;
    int pmfnUnwind;                                 // RVA
    int pForwardCompat;                             // RVA
    int pCatchableTypeArray;                        // RVA
} EH_THROWINFO_RELATIVE, *PEH_THROWINFO_RELATIVE;

typedef struct _EH_THROWINFO_ABSOLUTE64
{
    DWORD attributes;
    PVOID __ptr64 pmfnUnwind;
    PVOID __ptr64 pForwardCompat;
    PEH_CATCHABLETYPEARRAY_ABSOLUTE64 __ptr64 pCatchableTypeArray;
} EH_THROWINFO_ABSOLUTE64, *PEH_THROWINFO_ABSOLUTE64;

// X86, CHPE
typedef struct _EH_THROWINFO_ABSOLUTE32
{
    DWORD attributes;
    PVOID __ptr32 pmfnUnwind;
    PVOID __ptr32 pForwardCompat;
    PEH_CATCHABLETYPEARRAY_ABSOLUTE32 __ptr32 pCatchableTypeArray;
} EH_THROWINFO_ABSOLUTE32, *PEH_THROWINFO_ABSOLUTE32;

// X64, ARM64, CHPE
typedef struct _EH_HANDLERTYPE_RELATIVE64
{
    DWORD adjectives;
    int dispType;                                   // RVA to EH_TYPEDESCRIPTOR64
    int dispCatchObj;
    int dispOfHandler;                              // RVA
    int dispFrame;
} EH_HANDLERTYPE_RELATIVE64, *PEH_HANDLERTYPE_RELATIVE64;

// ARM32
typedef struct _EH_HANDLERTYPE_RELATIVE32
{
    DWORD adjectives;
    int dispType;                                   // RVA to EH_TYPEDESCRIPTOR32
    int dispCatchObj;
    int dispOfHandler;                              // RVA
} EH_HANDLERTYPE_RELATIVE32, *PEH_HANDLERTYPE_RELATIVE32;

// X86
typedef struct _EH_HANDLERTYPE_ABSOLUTE32
{
    DWORD adjectives;
    PEH_TYPEDESCRIPTOR32 __ptr32 pType;
    int dispCatchObj;
    PVOID __ptr32 addressOfHandler;
} EH_HANDLERTYPE_ABSOLUTE32, *PEH_HANDLERTYPE_ABSOLUTE32;

// X64, ARM64, ARM32, CHPE
typedef struct _EH_TRYBLOCKMAP_RELATIVE
{
    int tryLow;
    int tryHigh;
    int catchHigh;
    int nCatches;
    int dispHandlerArray;                           // RVA
} EH_TRYBLOCKMAP_RELATIVE, *PEH_TRYBLOCKMAP_RELATIVE;

// X86
typedef struct _EH_TRYBLOCKMAP_ABSOLUTE32
{
    int tryLow;
    int tryHigh;
    int catchHigh;
    int nCatches;
    PEH_HANDLERTYPE_ABSOLUTE32 __ptr32 pHandlerArray;
} EH_TRYBLOCKMAP_ABSOLUTE32, *PEH_TRYBLOCKMAP_ABSOLUTE32;

// X64, ARM64, ARM32, CHPE: relative
typedef struct _EH_UNWINDMAP_RELATIVE
{
    int toState;
    int action;                                     // RVA
} EH_UNWINDMAP_RELATIVE, *PEH_UNWINDMAP_RELATIVE;

// X86
typedef struct _EH_UNWINDMAP_ABSOLUTE32
{
    int toState;
    PVOID __ptr32 action;
} EH_UNWINDMAP_ABSOLUTE32, *PEH_UNWINDMAP_ABSOLUTE32;

// X64, ARM64, ARM32, CHPE
typedef struct _EH_IPTOSTATEMAPENTRY
{
    DWORD Ip;                                       // RVA
    int State;
} EH_IPTOSTATEMAPENTRY, *PEH_IPTOSTATEMAPENTRY;

// X64, ARM64, ARM32: relative
typedef struct _EH_ESTYPELIST_RELATIVE
{
    int nCount;
    int dispTypeArray;                              // RVA
} EH_ESTYPELIST_RELATIVE, *PEH_ESTYPELIST_RELATIVE;

// X86
typedef struct _EH_ESTYPELIST_ABSOLUTE32
{
    int nCount;
    PEH_HANDLERTYPE_ABSOLUTE32 __ptr32 pTypeArray;
} EH_ESTYPELIST_ABSOLUTE32, *PEH_ESTYPELIST_ABSOLUTE32;

// X64, ARM64, ARM32, CHPE
// FH3
typedef struct _EH_FUNCINFO_RELATIVE
{
    DWORD magicNumber   : 29;
    DWORD bbtFlags      : 3;
    int maxState;
    int dispUnwindMap;                              // RVA
    DWORD nTryBlocks;
    int dispTryBlockMap;                            // RVA
    DWORD nIPMapEntries;
    int dispIPtoStateMap;                           // RVA
    int dispUnwindHelp;
    int dispESTypeList;                             // RVA
    int EHFlags;
} EH_FUNCINFO_RELATIVE, *PEH_FUNCINFO_RELATIVE;

// X86
typedef struct _EH_FUNCINFO_ABSOLUTE32
{
    DWORD magicNumber   : 29;
    DWORD bbtFlags      : 3;
    int maxState;
    PEH_UNWINDMAP_ABSOLUTE32 __ptr32 pUnwindMap;
    DWORD nTryBlocks;
    PEH_TRYBLOCKMAP_ABSOLUTE32 __ptr32 pTryBlockMap;
    DWORD nIPMapEntries;
    PVOID __ptr32 pIPtoStateMap;
    PEH_ESTYPELIST_ABSOLUTE32 __ptr32 pESTypeList;
    int EHFlags;
} EH_FUNCINFO_ABSOLUTE32, *PEH_FUNCINFO_ABSOLUTE32;

// X86
typedef struct _EH_REGISTRATIONNODE_X86
{
    struct _EH_REGISTRATIONNODE_X86* __ptr32 pNext;
    PVOID __ptr32 frameHandler;
    int state;
} EH_REGISTRATIONNODE_X86, *PEH_REGISTRATIONNODE_X86;

// X64, ARM64: just a frame pointer (ULONG_PTR)
// ARM32: just a frame pointer (ULONG)

// X64, ARM64: 64-bit pointers, 4 params
typedef struct _EH_EXCEPTION_PARAMS_RELATIVE64
{
    DWORD magicNumber;
    PVOID __ptr64 pExceptionObject;
    PEH_THROWINFO_RELATIVE __ptr64 pThrowInfo;
    PVOID __ptr64 pThrowImageBase;
} EH_EXCEPTION_PARAMS_RELATIVE64, *PEH_EXCEPTION_PARAMS_RELATIVE64;

// ARM32, CHPE: 32-bit pointers, 4 params
typedef struct _EH_EXCEPTION_PARAMS_RELATIVE32
{
    DWORD magicNumber;
    PVOID __ptr32 pExceptionObject;
    PEH_THROWINFO_RELATIVE __ptr32 pThrowInfo;
    PVOID __ptr32 pThrowImageBase;
} EH_EXCEPTION_PARAMS_RELATIVE32, *PEH_EXCEPTION_PARAMS_RELATIVE32;

// (theoretical) 64-bit absolute, 3 params
typedef struct _EH_EXCEPTION_PARAMS_ABSOLUTE64
{
    DWORD magicNumber;
    PVOID __ptr64 pExceptionObject;
    PEH_THROWINFO_ABSOLUTE64 __ptr64 pThrowInfo;
} EH_EXCEPTION_PARAMS_ABSOLUTE64, *PEH_EXCEPTION_PARAMS_ABSOLUTE64;

// X86: 32-bit absolute, 3 params
typedef struct _EH_EXCEPTION_PARAMS_ABSOLUTE32
{
    DWORD magicNumber;
    PVOID __ptr32 pExceptionObject;
    PEH_THROWINFO_ABSOLUTE32 __ptr32 pThrowInfo;
} EH_EXCEPTION_PARAMS_ABSOLUTE32, *PEH_EXCEPTION_PARAMS_ABSOLUTE32;

// FH4 headers (compressed format, all relative)
typedef struct _EH_FUNCINFO4_HEADER
{
    BYTE isCatch        : 1;
    BYTE isSeparated    : 1;
    BYTE BBT            : 1;
    BYTE UnwindMap      : 1;
    BYTE TryBlockMap    : 1;
    BYTE EHs            : 1;
    BYTE NoExcept       : 1;
    BYTE reserved       : 1;
} EH_FUNCINFO4_HEADER, *PEH_FUNCINFO4_HEADER;

typedef struct _EH_HANDLERTYPE4_HEADER
{
    BYTE adjectives     : 1;
    BYTE dispType       : 1;
    BYTE dispCatchObj   : 1;
    BYTE contIsRVA      : 1;
    BYTE contAddr       : 2;
    BYTE unused         : 2;
} EH_HANDLERTYPE4_HEADER, *PEH_HANDLERTYPE4_HEADER;

typedef struct _EH_SEPIPTOSTATEMAPENTRY4
{
    int addrStartRVA;
    int dispOfIPMap;
} EH_SEPIPTOSTATEMAPENTRY4, *PEH_SEPIPTOSTATEMAPENTRY4;

#pragma pack(pop)

#endif