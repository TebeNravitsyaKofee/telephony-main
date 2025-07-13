#include "settings.h"
#include <iostream>

int main(int argc, char *argv[])
{
    std::string arg;
    std::string arg2;

    if(argc==0)
    {
        return 1;
    }
    if (argc==2)
    {
        arg = argv[1];
    }
    if (argc==3)
    {
        arg = argv[1];
        arg2 = argv[2];
    }
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
            if (arg2 == "address")
            {
                reconfigureAddress();
            }
            else if (arg2 == "unique")
            {
                reconfigureUnique();
            }
            else if (arg2 == "port")
            {
                reconfigurePort();
            }
            else if (arg2 == "key")
            {
                reconfigureKey();
            }
        } 
        else 
        {
              std::cout << "Unknown argument: " << arg << arg2 << std::endl;
        }
    
    
    return 0;
}