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

std::string getAddressAPI()
{
    return addressAPI;
}

std::string getUnique()
{
    return unique;
}

std::string getKey()
{
    return key;
}

std::string getPort()
{
    return port;
}

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

void reconfigureAddress()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write API adress: ";
    std::cin >> value;
    settings ["Address="] = value;
    writeSettingsLoop(settings);
}

void reconfigureUnique()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write unique: ";
    std::cin >> value;
    settings ["Unique="] = value;
    writeSettingsLoop(settings);
}

void reconfigureKey()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write key: ";
    std::cin >> value;
    settings ["Key="] = value;
    writeSettingsLoop(settings);
}

void reconfigurePort()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write port: ";
    std::cin >> value;
    settings ["Port="] = value;
    writeSettingsLoop(settings);
}

void reconfigureSettings()
{
    readSettingsLoop();
    reconfigureAddress();
    reconfigureUnique();
    reconfigureKey();
    reconfigurePort();
    writeSettingsLoop(settings);
}

void partiallyReconfigure()
{
    readSettingsLoop();
    for (const auto& [key,value]:settings)
    {
        if (!value.empty()) continue;
        if(key==("Address="))
        {
            reconfigureAddress();
            writeSettingsLoop(settings);
            readSettingsLoop();
        }
        else if(key==("Unique="))
        {
            reconfigureUnique();
            writeSettingsLoop(settings);
            readSettingsLoop();
        }
        else if(key==("Key="))
        {
            reconfigureKey();
            writeSettingsLoop(settings);
            readSettingsLoop();
        }
        else if(key==("Port="))
        {
            reconfigurePort();
            writeSettingsLoop(settings);
            readSettingsLoop();
        }
    }
}

std::string readSettings()
{
    if (std::filesystem::exists("/home/mainuser/projects/telephony/connectionSettings.txt"))
    {
        std::ifstream file;
        file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
        if (file.is_open()) 
        {
            readSettingsLoop();
            
            for (const auto& [key,value]:settings)
            {
                if (!value.empty()) continue;
                else 
                {
                    return "precon";
                    break;
                }
            }

            addressAPI = settings ["Address="];
            unique = settings ["Unique="];
            key = settings ["Key="];
            port = settings ["Port="];
            
            file.close();
            return "good";
        } 
        else 
        {
            std::cerr << "Error: Unable to open the file.\n";
            return "fileError";
        }
    }  
    else
    {
        return "fileExistingError";
    }
}