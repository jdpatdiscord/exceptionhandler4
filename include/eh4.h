#pragma once

#ifndef EH4_H
#define EH4_H

#ifndef EH4_SOURCE_FILE
#include <Windows.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

typedef struct _EH4_SETTINGS
{
    int test;
} EH4_SETTINGS, *PEH4_SETTINGS;

EXTERN_C int EH4_Attach(PEH4_SETTINGS Settings);
EXTERN_C int EH4_Detach();

EXTERN_C void DebugPrint(const wchar_t* format, ...);

EXTERN_C LPTOP_LEVEL_EXCEPTION_FILTER g_pfnPreviousFilter;
EXTERN_C PEXCEPTION_POINTERS g_pStoredExceptionInformation;
EXTERN_C HANDLE g_hExceptionHandlerWatchdogProcess;
EXTERN_C HANDLE g_hCrashNotificationEvent;
EXTERN_C HANDLE g_hCrashNotificationEventHandle;
EXTERN_C HANDLE g_hStartedEvent;
EXTERN_C HANDLE g_hStartedEventHandle;
EXTERN_C HANDLE g_hCurrentProcess;
EXTERN_C HANDLE g_hWatchdogThread;
EXTERN_C BOOL g_bStopFlag;

#endif // #ifndef EH4_H