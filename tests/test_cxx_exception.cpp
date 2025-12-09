#include <eh4.h>

#include <stdexcept>

int main(int argc, char* argv[])
{
    EH4_SETTINGS settings;
    memset(&settings, 0, sizeof(settings));
    settings.sem = SEM_FAILCRITICALERRORS;
    settings.callPreviousHandler = FALSE;

    EH4_Attach(&settings);

    throw std::runtime_error("Testing testing testing");

    return 0;
}