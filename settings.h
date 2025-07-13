#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>

std::string getAddressAPI();
std::string getUnique();
std::string getKey();
std::string getPort();

void reconfigureAddress();
void reconfigureUnique();
void reconfigurePort();
void reconfigureKey();

std::string readSettings();

#endif