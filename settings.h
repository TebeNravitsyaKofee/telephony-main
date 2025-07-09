#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>



void readSettingsLoop();
void writeSettingsLoop(std::map<std::string,std::string> settings);


void reconfigureSettings();
void readSettings();

#endif