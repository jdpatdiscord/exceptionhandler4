#include <eh4.h>

int main(int argc, char* argv[])
{
    EH4_SETTINGS settings;
    memset(&settings, 0, sizeof(settings));
    settings.sem = SEM_FAILCRITICALERRORS;
    settings.callPreviousHandler = FALSE;

    EH4_Attach(&settings);

#if _MSC_VER && !__clang__
    *(int*)(0) = 0;
#else
    __builtin_trap();
#endif

    return 0;
}