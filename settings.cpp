#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>
#include <string>
#include <ctime>
#include "settings.h"

static std::string addressAPI;
static std::string unique;
static std::string key;
static std::string port;
static std::map<std::string,std::string> lines;

std::map<std::string,std::string> settings
{
    {"Address=",""},
    {"Unique=",""},
    {"Key=",""},
    {"Port=",""}
};

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

//use this to save cuurent map into a file
void writeLinesFromMap(std::map<std::string,std::string> final_lines)
{
    std::ofstream file("/home/mainuser/projects/telephony/lines.txt");
    for (const auto& [key,value]:final_lines)
    {
        file<<key<<value<<"\n";
    }
}

//loop, which is used to read lines from a text file, use it to re-write inner static map
void readLinesLoop()
{
    lines.clear();
    std::string line;
    std::ifstream file;
    file.open("/home/mainuser/projects/telephony/lines.txt");
    while (std::getline(file, line))
    {
        size_t pos = line.find('=');
        std::string key = line.substr(0,pos+1);
        std::string value = line.substr(pos+1);
        lines.insert({key,value});
    }
}

std::map<std::string,std::string> drawLines()
{
    readLinesLoop();
    return lines;
}

//only use this for lines program gets from Mango
void initializeLines(std::map<std::string,std::string> actual_lines)
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
        else
        {
            final_lines.insert({pair.first,pair.second});
        }
    }
    writeLinesFromMap(final_lines);
    readLinesLoop();
}

//use this initializing lines at the start, checking for file existance
std::string readLines()
{
    if (std::filesystem::exists("/home/mainuser/projects/telephony/lines.txt"))
    {
        std::ifstream file;
        file.open("/home/mainuser/projects/telephony/lines.txt");
        if (file.is_open()) 
        {
            readLinesLoop();
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



void processChunks(const std::vector<std::string>& tokens) 
{
    std::map<int, std::string> result;
    std::vector<int> buffer;

    auto parseRange = [](const std::string& token, std::vector<int>& output) 
    {
        size_t dashPos = token.find('-');
        if (dashPos == std::string::npos) 
        {
            output.push_back(std::stoi(token));
        } 
        else 
        {
            int start = std::stoi(token.substr(0, dashPos));
            int end   = std::stoi(token.substr(dashPos + 1));
            if (start > end) std::swap(start, end);

            for (int i = start; i <= end; ++i) 
            {
                output.push_back(i);
            }
        }
    };

    for (const auto& token : tokens) 
    {
        if (token == "on" || token == "off") 
        {
            // Записываем накопленные числа в map
            for (int num : buffer) 
            {
                result[num] = token;
            }
            buffer.clear();
        } 
        else 
        {
            parseRange(token, buffer);
        }
    }
    if (!buffer.empty()) 
    {
        std::cerr << "Ошибка: после диапазона или числа отсутствует 'on'/'off'\n";
    }

    std::map<std::string, std::string> resultStr;
    for (const auto& [key, value] : result) 
    {
        resultStr[std::to_string(key) + "="] = value;
    }
    
    //reading current lines, replace it later with readLines();
    readLinesLoop();
    for(const auto& pair : resultStr)
    {
        auto current_line = lines.find(pair.first);
        if (current_line != lines.end())
        {
            current_line->second = pair.second;
        }
        else
        {
            std::cout << "line " << pair.first << " not found\n";
        }
    }

    writeLinesFromMap(lines);

    //for debugging, remove later
    /*for (const auto& [key, value] : resultStr) 
    {
        std::cout << key << "=" << value << "\n";
    }
        */
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

int appendLog(std::string text)
{
    if (std::filesystem::exists("/home/mainuser/projects/telephony/log.txt"))
    {
        std::time_t now = std::time(0); 

        // Convert time_t to a human-readable string (local time)
        char* dt_local = std::ctime(&now);
        std::ofstream log_file("/home/mainuser/projects/telephony/log.txt", std::ios::app);

        // Check if the file was successfully opened
        if (log_file.is_open()) 
        {
            log_file << "\n" << dt_local << "  " << text << std::endl;
            log_file.close();
        } 
        else 
        {
            std::cerr << "Error: Unable to open log file\n";
        }
    }
    else
    {
        std::ofstream new_file("/home/mainuser/projects/telephony/log.txt");
        new_file << "Log started" << std::endl;
        new_file.close();
        appendLog(text);
    }
    return 0;
}     