#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>

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

void reconfigureAddress(std::map<std::string,std::string> settings)
{
    std::cout << "Write API adress: ";
    std::cin >> addressAPI;
    settings ["Address="] = addressAPI;
    writeSettingsLoop(settings);
}

void reconfigureUnique(std::map<std::string,std::string> settings)
{
    
    std::cout << "Write unique: ";
    std::cin >> unique;
    settings ["Unique="] = unique;
    writeSettingsLoop(settings);
}

void reconfigureKey(std::map<std::string,std::string> settings)
{
    std::cout << "Write key: ";
    std::cin >> key;
    settings ["Key="] = key;
    writeSettingsLoop(settings);
}

void reconfigurePort(std::map<std::string,std::string> settings)
{
    std::cout << "Write port: ";
    std::cin >> port;
    settings ["Port="] = port;
    writeSettingsLoop(settings);
}

void reconfigureSettings()
{
    
  
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
                    reconfigureAddress(settings);
                    readSettingsLoop();
                }
                else if(key==("Unique="))
                {
                    reconfigureUnique(settings);
                    readSettingsLoop();
                }
                else if(key==("Key="))
                {
                    reconfigureKey(settings);
                    readSettingsLoop();
                }
                else if(key==("Port="))
                {
                    reconfigurePort(settings);
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
int main()
{
    readSettings();
}