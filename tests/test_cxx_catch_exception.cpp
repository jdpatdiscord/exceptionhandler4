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
        throw std::runtime_error("Testing testing testing");
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}