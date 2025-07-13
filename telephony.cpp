#include "settings.h"
#include <iostream>

int main(int argc, char *argv[])
{
    if(argc<10)
    {
        std::cerr <<"123";
        return 1;
    }
        std::string arg = argv[1];
        std::string arg2 = argv[2];
        
        if (arg == "settings") 
        {
            if(readSettings()=="good")
            {
                std::cout << "API adress = " << getAddressAPI() << std::endl;
                std::cout << "Unique = " << getUnique() << std::endl;
                std::cout << "Port = " << getPort() << std::endl;
                std::cout << "Key = " << getKey() << std::endl;
            }
            else if (readSettings()=="precon")
            {
                int choice;
                std::cout << "Partial reconfigure required, do it?" << "1 - Yes\n" << "2 - No\n";
                std::cin >> choice;
                if(choice == 1)
                {
                    partialReconfigure();
                }
            }
            else if (readSettings()=="fileExistingError")
            {
                std::cout << "File not found, reconfigure required";
                reconfigureSettings();
            }
            else if (readSettings()=="fileError")
            {
                std::cout << "File reading error, quitting...";
            }
            
        } 
        else if (arg == "recon") 
        {
            std::cout << "File specified: " << std::endl;
        } 
        else 
        {
              std::cout << "Unknown argument: " << arg << std::endl;
        }
    
    
    return 0;
}