#include <iostream>
#include <fstream>
#include <filesystem>

std::string addressAPI;
std::string unique;
std::string key;
std::string port;

void reconfigureAddress()
{
    std::string addressAPIvalue;
    std::cout << "Write API adress: ";
    std::cin >> addressAPI;
    addressAPIvalue = "Address=" + addressAPI +"\n";
    
    file << addressAPIvalue;
}

void reconfigureUnique()
{
    std::string uniqueValue;
    std::cout << "Write unique: ";
    std::cin >> unique;
    uniqueValue = "Unique=" + unique +"\n";
    std::fstream file;
    file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
    file << uniqueValue;
}

void reconfigureKey()
{
    std::string keyValue;
    std::cout << "Write key: ";
    std::cin >> key;
    keyValue = "Key=" + key +"\n";
    std::fstream file;
    file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
    file << keyValue;
}

void reconfigurePort()
{
    std::string portValue;
    std::cout << "Write port: ";
    std::cin >> port;
    portValue = "Port=" + port;
    std::fstream file;
    file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
    file << portValue;
}

void reconfigureSettings()
{
    std::ifstream file;
    file.open("/home/mainuser/projects/telephony/connectionSettings.txt");
    file.clear();
    reconfigureAddress();
    reconfigureUnique();
    reconfigureKey();
    reconfigurePort();    
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
            int check = 0;
            std::string line;
            while (std::getline(file, line))
            {
                check+=1;
            }
            if (check<4)
            {
                std::cout << "Error: Settings missing, please reconfigure";
                reconfigureSettings(); 
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