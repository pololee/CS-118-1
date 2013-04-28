/**@file http-common.h
* common functions and common parameters
* void *getIPAddr(struct sockaddr *sa);
	return IP address(IPv4 or IPv6)
* int iniServerListen(const char *port);
	Brief: create socket, bind it to port, listen
	return fileDescriptor of the socket, otherwise return <0
* int clientConnectToRemote(const char *host, const char *port);
	Brief: create socket, connect to remote host
	return fileDescri
* int getDatafromHost(int remoteFD, std::string &result);
	Brief: Get All data from remote host
	return 0 if success, otherwise return <0
*@author Xiao Li (pololee@cs.ucla.edu)
*@data 2013/4/26
*/

#include "http-common.h"

using namespace std;

void *getIPAddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Similar to the example in Beej's Guide to Networking Programming
int iniServerListen(const char *port)
{
	struct addrinfo hints, *res;
	int addr_status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; //(either IPv4 or IPv6)
  hints.ai_socktype = SOCK_STREAM; 
  hints.ai_flags = AI_PASSIVE; //return socket addresses will be suitable for bind()

  addr_status = getaddrinfo(NULL, port, &hints, &res);
  if (addr_status != 0)
  {
    fprintf(stderr, "Cannot get info\n");
    return -1;
  }

  // Loop through results, connect to first one we can
  struct addrinfo *p;
	int sock_fd;
  for (p = res; p != NULL; p = p->ai_next)
  {
    // Create the socket
    sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock_fd < 0)
    {
      perror("server: cannot open socket");
      continue;
    }

    // Set socket options
		// set SO_REUSEADDR on a socket to true(1)
    int yes = 1;
    int opt_status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (opt_status == -1)
    {
      perror("server: setsockopt");
      exit(1);
    }

    // Bind the socket to the port
		// associate a socket with an IP address and port number
    int bind_status = bind(sock_fd, p->ai_addr, p->ai_addrlen);
    if (bind_status != 0)
    {
      close(sock_fd);
      perror("server: Cannot bind socket");
      continue;
    }

    // Bind the first one we can
    break;
  }

	// No binds happened
  if (p == NULL)
  {
    fprintf(stderr, "server: failed to bind\n");
    return -2;
  }

	char s[INET6_ADDRSTRLEN];
   inet_ntop(p->ai_family, getIPAddr((struct sockaddr *)p->ai_addr), s, sizeof s);
   printf("proxy IP: %s\n", s);
    printf("proxy Port: %s\n", port);
    
  // Don't need the structure with address info any more
  freeaddrinfo(res);

  // Start listening
  // BACKLOG: the number of connections allowed on the incoming queue 
  if (listen(sock_fd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

	cout<<"Start listening"<<endl;
  return sock_fd;
}



int clientConnectToRemote(const char *host,const char *port)
{
  struct addrinfo hints, *res;
  int sock_fd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  //fprintf(stderr, "%s %s\n", host, port);
  int addr_status = getaddrinfo(host, port, &hints, &res);
  if (addr_status != 0)
  {
    fprintf(stderr, "Cannot get info\n");
    return -1;
  }

  // Loop through results, connect to first one we can
  struct addrinfo *p;
  for (p = res; p != NULL; p = p->ai_next)
  {
    // Create the socket
    sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock_fd < 0)
    {
      perror("client: cannot open socket");
      continue;
    }

    // Make connection
    int connect_status = connect(sock_fd, p->ai_addr, p->ai_addrlen);
    if (connect_status < 0)
    {
      close(sock_fd);
      perror("client: connect");
      continue;
    }
    break;
  }

  // No binds happened
  if (p == NULL)
  {
    fprintf(stderr, "client: failed to connect\n");
    return -2;
  }

  char s[INET6_ADDRSTRLEN];
  inet_ntop(p->ai_family, getIPAddr((struct sockaddr *)p->ai_addr), s, sizeof s);
  fprintf(stderr, "client: connecting to %s\n", s);

  // Don't need the structure with address info any more
  freeaddrinfo(res);

  return sock_fd;
}

int getDatafromHost(int remoteFD, string &result)
{
	while(true)
	{
		char recvBuf[BUFSIZE];
		int numRecv = recv(remoteFD, recvBuf, sizeof(recvBuf), 0);
		if(numRecv < 0)
		{
			perror("recv");
			return -1;
		}
		else if(numRecv == 0)
		{
			break;
		}
		result.append(recvBuf, numRecv);
	}
	return 0;
}















