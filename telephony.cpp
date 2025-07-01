#include <iostream>
#include <fstream>
#include <filesystem>

std::string addressAPI = "Address=";
std::string unique = "Unique=";
std::string key = "Key=";
std::string port = "Port=";


void reconfigureSettings()
{
    std::string addressAPIvalue;
    std::string uniqueValue;
    std::string keyValue;
    std::string portValue;

    std::cout << "Write API adress: ";
    std::cin >> addressAPIvalue;
    addressAPIvalue = addressAPI + addressAPIvalue +"\n";

    std::cout << "Write unique: ";
    std::cin >> uniqueValue;
    uniqueValue = unique + uniqueValue +"\n";

    std::cout << "Write key: ";
    std::cin >> keyValue;
    keyValue = key + keyValue+"\n";

    std::cout << "Write port: ";
    std::cin >> portValue;
    portValue = port + portValue;

    std::ofstream file ("connectionSettings.txt");
    file << addressAPIvalue;
    file << uniqueValue;
    file << keyValue;
    file << portValue;

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