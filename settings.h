#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>
#include <vector>

std::string getAddressAPI();
std::string getUnique();
std::string getKey();
std::string getPort();
std::string getSQLhost();
std::string getSQLport();
std::string getSQLdb();
std::string getSQLuser();
std::string getSQLpass();
std::map<std::string,std::string> drawLines();



void partialReconfigure();
void partialSQLReconfigure();
void reconfigureAddress();
void reconfigureUnique();
void reconfigurePort();
void reconfigureKey();
void reconfigureSQLhost();
void reconfigureSQLport();
void reconfigureSQLdb();
void reconfigureSQLdb(std::string dbname);
void reconfigureSQLuser();
void reconfigureSQLpass();
void reconfigureSettings();
void reconfigureSQLSettings();
void initializeLines(std::map<std::string,std::string> set);
std::string readSettings();
std::string readSQLSettings();
void processChunks(const std::vector<std::string>& toc);

int appendLog(std::string text);

#endif