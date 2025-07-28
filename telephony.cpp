#include "settings.h"
#include "connector.h"
#include <iostream>

int main(int argc, char *argv[])
{
    std::string arg1;
    std::string arg2;
    
    sslInit();
    getLines();
    //считаем количество аргументов, чтобы не возникала ошибка ссылания на несуществующий поинтер
    if(argc==0)
    {
        return 1;
    }
    if (argc==2)
    {
        arg1 = argv[1];
    }
    if (argc==3)
    {
        arg1 = argv[1];
        arg2 = argv[2];
    }

    if (arg1 == "settings") 
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
        else if (readSettings()=="fileExistanceError")
        {
            std::cout << "File not found, reconfigure required";
            reconfigureSettings();
        }
        else if (readSettings()=="fileError")
        {
            std::cout << "File reading error";
        }   
    } 
    else if (arg1 == "recon") 
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
        std::cout << "Unknown argument: " << std::endl;
    }
     
    return 0;
}