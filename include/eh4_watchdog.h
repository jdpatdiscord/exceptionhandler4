#pragma once

#ifndef EH4_WATCHDOG_H
#define EH4_WATCHDOG_H

#include <phnt.h>

#include <eh4.h>

EXTERN_C
int
EH4_ReadExceptionPointers(
    _In_  HANDLE hProcess, 
    _Out_ PEXCEPTION_POINTERS pLocalExceptionPointers,
    _In_  LPVOID pRemoteExceptionPointers
    );

EXTERN_C
int
EH4_ProcessException(
    _In_ HANDLE hProcess, 
    _In_ LPVOID pStoredExceptionInformation
    );

EXTERN_C
int
EH4_ProcessCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
    );

EXTERN_C
int
EH4_ProcessMSCCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
    );

EXTERN_C
int
EH4_ProcessGNUCXXException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
    );

///// Unwind //////////////////////////////////////////////////////////////////

// Attempts to unwind the stack of an exception and gather as much info as
// possible. Uses Microsoft's dbghelp.lib to do it.
EXTERN_C
int
EH4_UnwindExceptionDbghelp(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
    );

// Attempts to unwind the stack of an exception and gather as 
// much info as possible. Uses this project's PDB/DWARF 
// parsing and stack unwinding to do it.
EXTERN_C
int
EH4_UnwindException(
    _In_ HANDLE hProcess,
    _In_ PEXCEPTION_POINTERS pExceptionPointers
    );

///// Utilities ///////////////////////////////////////////////////////////////

EXTERN_C
int
EH4_ReadRemoteStringA(
    _In_  HANDLE hProcess,
    _In_  PCVOID pRemoteString,
    _Out_writes_z_(cchBuffer) LPSTR pBuffer,
    _In_  SIZE_T cchBuffer
    );

#endif // #ifndef EH4_WATCHDOG_H