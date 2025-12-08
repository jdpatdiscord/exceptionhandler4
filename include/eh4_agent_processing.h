#pragma once

#ifndef EH4_AGENT_PROCESSING_H
#define EH4_AGENT_PROCESSING_H

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

#endif // #ifndef EH4_AGENT_PROCESSING_H