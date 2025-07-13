#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>

std::string getAddressAPI();
std::string getUnique();
std::string getKey();
std::string getPort();

void partialReconfigure();
void reconfigureAddress();
void reconfigureUnique();
void reconfigurePort();
void reconfigureKey();
void reconfigureSettings();

std::string readSettings();

#endif