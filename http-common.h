/**@file http-common.h
* common functions and common parameters
*@author Xiao Li (pololee@cs.ucla.edu)
*@data 2013/4/26
*/

// C++ Libraries
#include <iostream>
#include <string>
#include <sstream>
// C Libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

// C Network/Socket Libraries
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PROXY_SERVER_PORT "48670"
#define REMOTE_SERVER_PORT "80"
#define BACKLOG 100
#define BUFSIZE 2048

void *getIPAddr(struct sockaddr *sa);
int iniServerListen(const char *port);
int clientConnectToRemote(const char *host, const char *port);
int getContentLength(std::string & response);
int getDatafromHost(int remoteFD, std::string &result);
