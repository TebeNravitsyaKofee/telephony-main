#include <iostream>
#include <fstream>
#include <filesystem>



void readSettings()
{
    if (std::filesystem::exists("/home/mainuser/projects/telephony/connectionSettings.txt"))
    {
        std::cout << "file exists";
    }
    else
    {
        std::ofstream file ("connectionSettings.txt");
    }
}
int main()
{
    readSettings();
}