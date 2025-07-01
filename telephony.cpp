#include <iostream>
#include <fstream>
#include <filesystem>

std::string addressAPI;
std::string unique;
std::string key;
std::string port;


void reconfigureSettings()
{
    std::string addressAPIvalue;
    std::string uniqueValue;
    std::string keyValue;
    std::string portValue;

    std::cout << "Write API adress: ";
    std::cin >> addressAPI;
    addressAPIvalue = "Address=" + addressAPI +"\n";

    std::cout << "Write unique: ";
    std::cin >> unique;
    uniqueValue = "Unique=" + unique +"\n";

    std::cout << "Write key: ";
    std::cin >> key;
    keyValue = "Key=" + key +"\n";

    std::cout << "Write port: ";
    std::cin >> port;
    portValue = "Port=" + port;

    std::ofstream file ("connectionSettings.txt");
    file << addressAPIvalue;
    file << uniqueValue;
    file << keyValue;
    file << portValue;
    file.close();
}

void readSettings()
{
    if (std::filesystem::exists("/home/mainuser/projects/telephony/connectionSettings.txt"))
    {
        std::cout << "File exists";
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