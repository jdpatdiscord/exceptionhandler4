#pragma once

#ifndef EH4_H
#define EH4_H

#ifndef EH4_SOURCE_FILE
#include <Windows.h>
#endif

#include <stdint.h>

typedef struct _EH4_SETTINGS
{
    int test;
} EH4_SETTINGS, *PEH4_SETTINGS;

EXTERN_C int EH4_Attach(PEH4_SETTINGS Settings);
EXTERN_C int EH4_Detach();

#endif