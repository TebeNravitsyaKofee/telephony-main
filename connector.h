#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <string>


void sslInit();
int getLines();
void startHttpServer(int port);
void displayCalls();

const char* PID_FILE;

bool isServerRunning();
int startServer();
int stopServer();





#endif