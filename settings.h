#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>

std::string getAddressAPI();
std::string getUnique();
std::string getKey();
std::string getPort();

std::string reconfigureAddress();

void readSettings();

#endif