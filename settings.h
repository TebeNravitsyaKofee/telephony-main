#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>
#include <vector>

std::string getAddressAPI();
std::string getUnique();
std::string getKey();
std::string getPort();
std::map<std::string,std::string> drawLines();



void partialReconfigure();
void reconfigureAddress();
void reconfigureUnique();
void reconfigurePort();
void reconfigureKey();
void reconfigureSettings();
void initializeLines(std::map<std::string,std::string> set);
std::string readSettings();
void processChunks(const std::vector<std::string>& toc);

int appendLog(std::string text);

#endif