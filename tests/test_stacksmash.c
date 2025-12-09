#include <eh4.h>

EXTERN_C void WreckIt(void);

int main(int argc, char* argv[])
{
    EH4_SETTINGS settings;
    memset(&settings, 0, sizeof(settings));
    settings.sem = SEM_FAILCRITICALERRORS;
    settings.callPreviousHandler = FALSE;

    EH4_Attach(&settings);

    WreckIt();

    return 0;
}