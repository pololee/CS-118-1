// C++ Libraries
#include <iostream>
#include <string>
#include <map>

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

#include "http-request.h"
#include "http-common.h"
#include "compat.h"

using namespace std;
typedef map<string, string> CacheTable;

typedef struct PthreadPara
{
	int clientFD;
	CacheTable *cacheTable;
	pthread_mutex_t *mutex;
}PthreadPara;