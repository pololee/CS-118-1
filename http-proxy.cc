/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/** @file http-proxy.cc
*@author Xiao Li (pololee@cs.ucla.edu)
*@data 2013/4/26
*/

#include <iostream>
<<<<<<< HEAD
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

=======
#include "http-common.h"
>>>>>>> 598a18f9465f283e2e50ef5731b10fb6b560ab4c
using namespace std;

int get_data(string hostname, string &result_buffer)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char buf[256];
    int byte_count;
    
    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(hostname, "http", &hints, &res );
    if (status !=0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    connect(sockfd, res->ai_addr, res->ai_addrlen);
    //char hostname[NI_MAXHOST] = "";
    //int error = getnameinfo(res->ai_addr, res->ai_addrlen, hostname, NI_MAXHOST, NULL, 0,0);
    //if (error !=0)
    //{
    //    fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(error));
    //}
    //if (hostname!='\0')
    //    printf("hostname: %s\n", hostname);
    while(true)
    {
        byte_count = recv(sockfd, buf, sizeof buf, 0);
        cout<<byte_count<<endl;
        if (byte_count<0)
        {
            perror("recv");
            return -1;
        }
        else if (byte_count==0)
        {
            break;
        }
        result_buffer.append(buf,byte_count);
    }
    return 0;
}

int main (int argc, char *argv[])
{
<<<<<<< HEAD
    //test get pages
    string res;
    get_data("www.ucla.edu",res);
    cout<<res<<endl;
=======
  // command line parsing
  cout<<"hello, world!"<<endl;
  int proxySockFD = iniServerListen(PROXY_SERVER_PORT);
	cout<<"finish listen\n";
	cout<<"proxySockFD: "<<proxySockFD<<endl;
>>>>>>> 598a18f9465f283e2e50ef5731b10fb6b560ab4c
  return 0;
}
