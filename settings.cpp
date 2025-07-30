#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>
#include <string>
#include "settings.h"

static std::string addressAPI;
static std::string unique;
static std::string key;
static std::string port;

std::map<std::string,std::string> settings
{
    {"Address=",""},
    {"Unique=",""},
    {"Key=",""},
    {"Port=",""}
};

static std::map<std::string,std::string> lines;


std::string getAddressAPI()
{
    readSettings();
    return addressAPI;
}

std::string getUnique()
{
    readSettings();
    return unique;
}

std::string getKey()
{
    readSettings();
    return key;
}

std::string getPort()
{
    readSettings();
    return port;
}

void insertLines(std::map<std::string,std::string> actual_lines)
{
    std::map<std::string,std::string> current_lines, final_lines;

    std::string line;
    std::ifstream file;

    file.open("/home/mainuser/projects/telephony/lines.txt");
    
    while (std::getline(file, line))
    {
        size_t pos = line.find('=');
        std::string key = line.substr(0,pos+1);
        std::string value = line.substr(pos+1);
        current_lines.insert({key,value});
    }

    for(const auto& pair : actual_lines)
    {
        auto current_line = current_lines.find(pair.first);
        if (pair.first == current_line->first)
        {
            final_lines.insert({pair.first,current_line->second});
        }
    }
}

void readLinesLoop()
{

    std::string line;
    std::ifstream file;

    file.open("/home/mainuser/projects/telephony/lines.txt");
    
    while (std::getline(file, line))
    {
        
    }
}

void writeLinesFromMap()
{
    std::ofstream file("/home/mainuser/projects/telephony/connectionSettings.txt");
    for (const auto& [key,value]:lines)
    {
        file<<key<<value<<"\n";
    }
}



std::string readLines(std::vector<std::string> ext)
{


    if (std::filesystem::exists("/home/mainuser/projects/telephony/lines.txt"))
    {
        std::ifstream file;
        file.open("/home/mainuser/projects/telephony/lines.txt");
        if (file.is_open()) 
        {

            
            
            
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
        return "fileExistanceError";
    }
}

//метод для прочтения настроечного файла, интегрируется в уже созданную мапу, 
//игнорирует строки, которых нет в мапе
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

//метод для записи настроечного файла, перезаписывает файл в соответствии с мапой
void writeSettingsLoop(std::map<std::string,std::string> settings)
{
    std::ofstream file("/home/mainuser/projects/telephony/connectionSettings.txt");
    for (const auto& [key,value]:settings)
    {
        file<<key<<value<<"\n";
    }
}

//методы реконфигурации файла
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

void partialReconfigure()
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
        return "fileExistanceError";
    }
}