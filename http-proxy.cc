/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

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
    //test get pages
    string res;
    get_data("www.ucla.edu",res);
    cout<<res<<endl;
  return 0;
}
