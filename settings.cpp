#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include "settings.h"

std::string addressAPI;
std::string unique;
std::string key;
std::string port;

std::map<std::string,std::string> settings
    {
        {"Address=",""},
        {"Unique=",""},
        {"Key=",""},
        {"Port=",""}
    };

void readSettingsLoop()
{
    std::string line;
    std::ifstream file;
        file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
    while (std::getline(file, line))
    {
        size_t pos = line.find('=');
        std::string key = line.substr(0,pos+1);
        std::string value = line.substr(pos+1);
        settings[key] = value;
    }
}
void writeSettingsLoop(std::map<std::string,std::string> settings)
{
    std::ofstream file("/home/mainuser/projects/telephony/connectionSettings.txt");
    for (const auto& [key,value]:settings)
            {
                file<<key<<value<<"\n";
            }
}

std::string reconfigureAddress(std::map<std::string,std::string> settings)
{
    std::cout << "Write API adress: ";
    std::cin >> addressAPI;
    return addressAPI;
}

std::string reconfigureUnique(std::map<std::string,std::string> settings)
{
    
    std::cout << "Write unique: ";
    std::cin >> unique;
    return unique;
}

std::string reconfigureKey(std::map<std::string,std::string> settings)
{
    std::cout << "Write key: ";
    std::cin >> key;
    return key;
}

std::string reconfigurePort(std::map<std::string,std::string> settings)
{
    std::cout << "Write port: ";
    std::cin >> port;
    return port;
}

void reconfigureSettings()
{
    std::string address = reconfigureAddress(settings);
    std::string unique = reconfigureUnique(settings);
    std::string key = reconfigureKey(settings);
    std::string port = reconfigurePort(settings);
    settings ["Address="] = address;
    settings ["Unique="] = unique;
    settings ["Key="] = key;
    settings ["Port="] = port;
    writeSettingsLoop(settings);
}



void readSettings()
{
    if (std::filesystem::exists("/home/mainuser/projects/telephony/connectionSettings.txt"))
    {
        std::cout << "File exists";
        std::ifstream file;
        file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
        if (file.is_open()) 
        {
            readSettingsLoop();
            for (const auto& [key,value]:settings)
            {
                if (!value.empty()) continue;

                if(key==("Address="))
                {
                    
                    settings ["Address="] = reconfigureAddress(settings);
                    writeSettingsLoop(settings);
                    readSettingsLoop();
                }
                else if(key==("Unique="))
                {
                    settings ["Unique="] = reconfigureUnique(settings);
                    writeSettingsLoop(settings);
                    
                    readSettingsLoop();
                }
                else if(key==("Key="))
                {
                    settings ["Key="] = reconfigureKey(settings);
                    
                    writeSettingsLoop(settings);
                    readSettingsLoop();
                }
                else if(key==("Port="))
                {
                    settings ["Port="] = reconfigurePort(settings);
                    
                    writeSettingsLoop(settings);
                    readSettingsLoop();
                }
            }
            
            file.close();
        } 
        else 
        {
            std::cerr << "Error: Unable to open the file." << std::endl;
        }
    }  
    else
    {
        std::cout << "File not found, configuring settings";
        reconfigureSettings(); 
    }
}