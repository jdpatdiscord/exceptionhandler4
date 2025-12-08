#include <eh4.h>

#include <stdexcept>
#include <iostream>

int main(int argc, char* argv[])
{
    EH4_SETTINGS settings;
    memset(&settings, 0, sizeof(settings));

    EH4_Attach(&settings);

    try
    {
        throw;
    }
    catch(...)
    {
        std::cout << "should never be reached" << std::endl;
    }

    return 0;
}