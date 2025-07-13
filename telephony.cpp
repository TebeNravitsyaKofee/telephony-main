#include "settings.h"
#include <iostream>

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) 
    {
        std::string arg = argv[i];
        if (arg == "status") 
        {
            readSettings();
            std::cout << "123 mode enabled." << std::endl;
        } 
        else if (arg == "-f" && i + 1 < argc) 
        {
            std::cout << "File specified: " << argv[++i] << std::endl;
        } 
        else 
        {
              std::cout << "Unknown argument: " << arg << std::endl;
        }
    }
    
    return 0;
}