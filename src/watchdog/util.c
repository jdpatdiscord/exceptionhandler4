#include <eh4_watchdog.h>

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