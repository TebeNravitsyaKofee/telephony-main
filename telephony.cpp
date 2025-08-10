#include "settings.h"
#include "connector.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

//this is for line testing
/*
std::map<std::string,std::string> set
    {   
        {"101=","off"},
        {"102=","off"},
        {"103=","off"}
    };
*/



int main(int argc, char *argv[])
{
    //sslInit();
    //getLines();
    
        

    //считаем количество аргументов, чтобы не возникала ошибка ссылания на несуществующий поинтер
    /*if(argc==0)
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
    }*/

    //command line args processing
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        std::cout << "No arguments provided.\n";
        return 1;
    }
    std::string command = args[0];
    std::string subcommand = args.size() > 1 ? args[1] : "";

    if (command == "settings") 
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
    else if (command == "recon") 
    {
        if (subcommand == "address")
        {
            reconfigureAddress();
        }
        else if (subcommand == "unique")
        {
            reconfigureUnique();
        }
        else if (subcommand == "port")
        {
            reconfigurePort();
        }
        else if (subcommand == "key")
        {
            reconfigureKey();
        }
    } 
    else if (command == "lines")
    {
        std::vector<std::string> line_args = args;

        if (subcommand == "recon")
        {
            line_args.erase(line_args.begin(), line_args.begin() + 2);
            processChunks(line_args);
        }

        else
        {
            std::map<std::string,std::string> lines_map;
            lines_map = drawLines();
            //move to debug version later
            /*
            for (const auto& [key,value]:lines_map)
            {
                std::cout<<key<<value<<"\n";
            }  
            */
            
        }  
    }
    else 
    {
        std::cout << "Unknown argument: " << std::endl;
    }
     
    return 0;
}