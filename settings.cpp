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

std::string reconfigureAddress()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write API adress: ";
    std::cin >> value;
    settings ["Address="] = value;
    writeSettingsLoop(settings);
}

std::string reconfigureUnique()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write unique: ";
    std::cin >> value;
    settings ["Unique="] = value;
    writeSettingsLoop(settings);
}

std::string reconfigureKey()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write key: ";
    std::cin >> value;
    settings ["Key="] = value;
    writeSettingsLoop(settings);
}

std::string reconfigurePort()
{
    readSettingsLoop();
    std::string value;
    std::cout << "Write port: ";
    std::cin >> value;
    settings ["Port="] = value;
    return value;
}

void reconfigureSettings()
{
    readSettingsLoop();
    std::string address = reconfigureAddress();
    std::string unique = reconfigureUnique();
    std::string key = reconfigureKey();
    std::string port = reconfigurePort();
    writeSettingsLoop(settings);
}



void readSettings()
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

                if(key==("Address="))
                {
                    
                    settings ["Address="] = reconfigureAddress();
                    writeSettingsLoop(settings);
                    readSettingsLoop();
                }
                else if(key==("Unique="))
                {
                    settings ["Unique="] = reconfigureUnique();
                    writeSettingsLoop(settings);
                    
                    readSettingsLoop();
                }
                else if(key==("Key="))
                {
                    settings ["Key="] = reconfigureKey();
                    
                    writeSettingsLoop(settings);
                    readSettingsLoop();
                }
                else if(key==("Port="))
                {
                    settings ["Port="] = reconfigurePort();
                    
                    writeSettingsLoop(settings);
                    readSettingsLoop();
                }
            }

            addressAPI = settings ["Address="];
            unique = settings ["Unique="];
            key = settings ["Key="];
            port = settings ["Port="];
            
            file.close();
        } 
        else 
        {
            std::cerr << "Error: Unable to open the file.\n";
        }
    }  
    else
    {
        std::cout << "File not found, configuring settings\n";
        reconfigureSettings(); 
        readSettingsLoop();
        addressAPI = settings ["Address="];
        unique = settings ["Unique="];
        key = settings ["Key="];
        port = settings ["Port="];
    }
}