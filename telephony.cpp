#include "settings.h"
#include "connector.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <unistd.h>
#include <csignal>



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
    sslInit();

    

    // при завершении сервера удаляем PID файл
    

    std::vector<std::string> args(argv + 1, argv + argc);

    #ifdef DEBUG
    //debug command line processing
    if (argc <= 1) {

        args = {"lines"};
    } 
    else 
    {
        for (int i = 1; i < argc; ++i) 
        {
            args.push_back(argv[i]);
        }
    }
    #else
    //command line args processing
    if (args.empty()) 
    {
        std::cout << "No arguments provided.\n";
    }
    #endif
    
    while (true)
    {
        //command var has to contain something on start, otherwise it breaks
        //in this case it contains program name and no args
        if (args.empty())
        {
            std::string inputLine;
            std::getline(std::cin, inputLine);

            if(inputLine.empty()) continue;
            if(inputLine == "exit") break;

            size_t pos = 0;
            while ((pos = inputLine.find(' ')) != std::string::npos) 
            {
                args.push_back(inputLine.substr(0, pos));
                inputLine.erase(0, pos + 1);
            }
            args.push_back(inputLine);
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
                //drawing lines to console
                std::map<std::string,std::string> lines_map;
                lines_map = drawLines();
                for (const auto& [key,value]:lines_map)
                {
                    std::cout<<key<<value<<"\n";
                }  
            }  
        }
        else if (command == "calls")
        {
            displayCalls();
        }
        else if (command == "server")
        {
            if (subcommand == "status")
            {
                if(isServerRunning())
                {
                    std::cout << "Server running." << std::endl;
                }
                else
                {
                    std::cout << "Server shutdown." << std::endl;
                }
            }
            else if (subcommand == "start")
            {
                startServer();
            }
            else if (subcommand == "stop")
            {
                stopServer();
            }
            else
            {
                std::cout << "Unknown command." << std::endl;
            }
        }
        else 
        {
            std::cout << "Unknown argument: " << std::endl;
        }
        args.clear();
    }


    return 0;
}

