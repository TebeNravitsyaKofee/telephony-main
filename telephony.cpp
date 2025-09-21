#include "settings.h"
#include "connector.h"
#include "Modules/odbc_module.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <unistd.h>
#include <csignal>



//this is for line testing
/*
std::map<std::string,std::string> set
    {   
        {"101=","off"},
        {"102=","off"},
        {"103=","off"}
    };
*/

    






int main(int argc, char *argv[])
{
    sslInit();
    stopServer();
    startServer();



    std::vector<std::string> args(argv + 1, argv + argc);

    #ifdef DEBUG
    //debug command line processing
    if (argc <= 1) {

        args = {"lines"};
    } 
    else 
    {
        for (int i = 1; i < argc; ++i) 
        {
            args.push_back(argv[i]);
        }
    }
    #else
    //command line args processing
    if (args.empty()) 
    {
        std::cout << "No arguments provided.\n";
    }
    #endif
    
    while (true)
    {
        //command var has to contain something on start, otherwise it breaks
        //in this case it contains program name and no args
        if (args.empty())
        {
            std::string inputLine;
            std::getline(std::cin, inputLine);

            if(inputLine.empty()) continue;
            if(inputLine == "exit") break;

            size_t pos = 0;
            while ((pos = inputLine.find(' ')) != std::string::npos) 
            {
                args.push_back(inputLine.substr(0, pos));
                inputLine.erase(0, pos + 1);
            }
            args.push_back(inputLine);
        }
        std::string command = args[0];
        std::string subcommand = args.size() > 1 ? args[1] : "";

        if (command == "settings") 
        {
            if(readSettings()=="good")
            {
                std::cout << "API adress = " << getAddressAPI() << std::endl;
                std::cout << "Unique = " << getUnique() << std::endl;
                std::cout << "Port = " << getPort() << std::endl;
                std::cout << "Key = " << getKey() << std::endl;
            }
            else if (readSettings()=="precon")
            {
                int choice;
                std::cout << "Partial reconfigure required, do it?\n" << "1 - Yes\n" << "2 - No\n";
                std::cin >> choice;
                if(choice == 1)
                {
                    partialReconfigure();
                }
            }
            else if (readSettings()=="fileExistanceError")
            {
                std::cout << "File not found, reconfigure required";
                reconfigureSettings();
            }
            else if (readSettings()=="fileError")
            {
                std::cout << "File reading error";
            }   
        }
        /*
        else if (command == "sqlsettings")
        {
            if(readSQLSettings()=="good")
            {
                std::cout << "SQL hostname = " << getSQLhost() << std::endl;
                std::cout << "SQL port = " << getSQLport() << std::endl;
                std::cout << "SQL database name = " << getSQLdb() << std::endl;
                std::cout << "SQL username = " << getSQLuser() << std::endl;
                std::cout << "SQL password is encrypted :)" << std::endl;
            }
            else if (readSQLSettings()=="precon")
            {
                int choice;
                std::cout << "Partial reconfigure required, do it?\n" << "1 - Yes\n" << "2 - No\n";
                std::cin >> choice;
                if(choice == 1)
                {
                    partialSQLReconfigure();
                }
            }
            else if (readSettings()=="fileExistanceError")
            {
                std::cout << "File not found, reconfigure required";
                reconfigureSQLSettings();
            }
            else if (readSettings()=="fileError")
            {
                std::cout << "File reading error";
            }   
        }
            */
        else if (command == "recon") 
        {
            if (subcommand == "address")
            {
                reconfigureAddress();
            }
            else if (subcommand == "unique")
            {
                reconfigureUnique();
            }
            else if (subcommand == "port")
            {
                reconfigurePort();
            }
            else if (subcommand == "key")
            {
                reconfigureKey();
            }
            else if (subcommand == "sqlhost")
            {
                reconfigureSQLhost();
            }
            else if (subcommand == "sqlport")
            {
                reconfigureSQLport();
            }
            else if (subcommand == "sqldb")
            {
                reconfigureSQLdb();
            }
            else if (subcommand == "sqluser")
            {
                reconfigureSQLuser();
            }
            else if (subcommand == "sqlpass")
            {
                reconfigureSQLpass();
            }
        }
        else if (command == "lines")
        {
            std::vector<std::string> line_args = args;
            if (subcommand == "recon")
            {
                line_args.erase(line_args.begin(), line_args.begin() + 2);
                processChunks(line_args);
            }
            else
            {
                //drawing lines to console
                std::map<std::string,std::string> lines_map;
                lines_map = drawLines();
                for (const auto& [key,value]:lines_map)
                {
                    std::cout<<key<<value<<"\n";
                }  
            }  
        }
        else if (command == "calls")
        {
            displayCalls();
        }
        else if (command == "server")
        {
            if (subcommand == "status")
            {
                if(isServerRunning())
                {
                    std::cout << "Server running." << std::endl;
                }
                else
                {
                    std::cout << "Server shutdown." << std::endl;
                }
            }
            else if (subcommand == "start")
            {
                startServer();
            }
            else if (subcommand == "stop")
            {
                stopServer();
            }
            else
            {
                std::cout << "Unknown command." << std::endl;
            }
        }
        else if (command == "sql")
        {
            if (subcommand == "settings")
            {
                int choice;
                std::cout << "SQL hostname = " << getSQLhost() << std::endl;
                std::cout << "SQL port = " << getSQLport() << std::endl;
                std::cout << "SQL database name = " << getSQLdb() << std::endl;
                std::cout << "SQL username = " << getSQLuser() << std::endl;
                std::cout << "SQL password is encrypted :)" << std::endl;
                std::cout << "Want to reconfigure?\nYes - 1\nNo - 2" << std::endl;   
                std::cin >> choice;
                if(choice == 1)
                {
                    std::cout << "Choose field no reconfigure:\n1. Host\n2. Port\n3. Username\n4. Password\n5. Database name\n 6. All" << std::endl;
                    std::cin >> choice;
                    switch (choice)
                    {
                        case 1: reconfigureSQLhost(); break;
                        case 2: reconfigureSQLport(); break;
                        case 3: reconfigureSQLuser(); break;
                        case 4: reconfigureSQLpass(); break;
                        case 5: reconfigureSQLdb(); break;
                        case 6: reconfigureSQLSettings(); break;
                        default: std::cout << "Invalid input" << std::endl; break;
                    }
                }
            }
            else if (subcommand == "connection")
            {
                PostgreSQLConnector connector;

                bool c = connector.connectToServer();
                bool cc = connector.connectToDB();
                if (!c)
                {
                    std::cout << "Cannot connect to SQL server" << std::endl;
                }
                else if (!cc)
                {
                    std::cout << "Cannot connect to defined database" << std::endl;
                }
            }
            else if (subcommand == "db")
            {
                PostgreSQLConnector connector;

                bool c = connector.connectToServer();

                if (c)
                {
                    if(!connector.databaseExists(getSQLdb()))
                    {
                        std::cout << "Database not found, want to create?\n1 - Yes\n2 - No" << std::endl;
                        int choice;
                        std::cin >> choice;
                        if(choice == 1)
                        {
                            std::cout << "Enter database name" << std::endl;
                            std::string dbname;
                            std::cin >> dbname;
                            if(connector.createDatabase(dbname))
                            {
                                reconfigureSQLdb(dbname);
                            }
                            else
                            {
                                std::cout << "Database not created, check connection with sql->connection" << std::endl;
                            }
                        }
                    }
                }
                else
                {
                    std::cout << "Cannot connect to SQL server, check connection with sql->connection" << std::endl;
                }
                
                
            }
            
        }
        else 
        {
            std::cout << "Unknown argument: " << std::endl;
        }
        args.clear();
    }


    return 0;
}

