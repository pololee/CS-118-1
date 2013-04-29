/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/** @file http-proxy.cc
*@author Xiao Li (pololee@cs.ucla.edu)
*@data 2013/4/26
*/

#include "http-proxy.h"

//@Brief: deal with communication between client and proxy
//      Get the request from client
//      Parse the request
//      Give back response to client(cache and new request)
int clientToProxy(int clientFD, CacheTable cacheTable, pthread_mutex_t *mutex);

//@brief: call ClientToProxy()
void * pthread_ClientToProxy(void * params);


#define DEBUG

int main (int argc, char *argv[])
{
    //Create proxy server and make it listen
    int proxySockFD = iniServerListen(PROXY_SERVER_PORT);
#ifdef DEBUG
    cout<<"proxySockFD: "<<proxySockFD<<endl;
#endif
    
    if (proxySockFD < 0) {
        fprintf(stderr, "Cannot make proxy server listen on port %s\n", PROXY_SERVER_PORT);
        return 1;
    }
    
    //Initialize Cache
    CacheTable cache;
    pthread_mutex_t cacheMutex;
    pthread_mutex_init(&cacheMutex, NULL);
#ifdef DEBUG
    cout<<"initialized cache!"<<endl;
#endif
    
    //Loop to accept the request
    while (true) {
        struct sockaddr_storage their_addr;
        char ipAddr[INET6_ADDRSTRLEN];
        socklen_t sin_size = sizeof(their_addr);
        
        //Accept connection
        int clientFD = accept(proxySockFD, (struct sockaddr *)&their_addr, &sin_size);
#ifdef DEBUG
        cout<<"----------------Request from client-----------------"<<endl;
        cout<<"clientFD: "<<clientFD<<endl;
#endif
        if (clientFD == -1) {
            perror("accept");
            continue;
        }
        
        inet_ntop(their_addr.ss_family, getIPAddr((struct sockaddr *)&their_addr), ipAddr, sizeof(ipAddr));
        printf("Proxy: got connection from %s\n", ipAddr);
        
        //Set up pthreads parameters
        PthreadPara *params = (PthreadPara*)malloc(sizeof(PthreadPara));
        params->clientFD = clientFD;
        params->cacheTable = &cache;
        params->mutex = &cacheMutex;
        
        pthread_t threadID;
        pthread_create(&threadID, NULL, pthread_ClientToProxy, (void *)params);
    }
    
    
    return 0;
}

int clientToProxy(int clientFD,CacheTable *cacheTable,pthread_mutex_t *mutex)
{
    //Get the request from client
    cout<<"------------------client To Proxy---------------------"<<endl;
	 string clientBuffer;
    while (memmem(clientBuffer.c_str(), clientBuffer.length(), "\r\n\r\n", 4) == NULL) {
        char buf[BUFSIZE];
        if (recv(clientFD, buf, sizeof(buf), 0) < 0) {
            perror("recv");
            return -1;
        }
        clientBuffer.append(buf);
    }

    #ifdef DEBUG
    cout<<"client request: "<<clientBuffer<<endl;
    #endif
    
    //Parse request from client
    HttpRequest clientReq;
    try {
        clientReq.ParseRequest(clientBuffer.c_str(), clientBuffer.length());
    } catch (ParseException ex)
    {
        printf("Exception raised: %s\n", ex.what());
        string clientResponse = "HTTP/1.1";
        
        string cmp = "Request is not GET";
        if (strcmp(ex.what(), cmp.c_str()) != 0) {
            clientResponse += " 400 Bad Request\r\n\r\n";
        }
        else{
            clientResponse += " 501 Not Implemented\r\n\r\n";
        }
        
        if (send(clientFD, clientResponse.c_str(), clientResponse.length(), 0) == -1) {
            perror("send");
        }
    }
    
    //Construct remote request
    size_t remoteReqLength = clientReq.GetTotalLength() + 1;
    char * remoteReq = (char *)malloc(remoteReqLength);
    clientReq.FormatRequest(remoteReq);
    
    string remoteHost;
    if(clientReq.GetHost().length() == 0)
    {
        remoteHost = clientReq.FindHeader("Host");
    }
    else{
        remoteHost = clientReq.GetHost();
    }
    
    #ifdef DEBUG
    cout<<"remoteHost: "<<remoteHost<<endl;
    #endif
    //----------------Go to get a response to the request--------------------
    //=====First check cache=======
    string remoteResponse;
    string fullPath = remoteHost + clientReq.GetPath();
    CacheTable::iterator it = cacheTable->find(fullPath);
    if (it != cacheTable->end()) {
        remoteResponse = it->second;

        #ifdef DEBUG
        cout<<"Found local cache"<<endl;
        cout<<"response: "<<it->second<<endl;
        #endif
    }
    else{
        
        //=================There is no local cache======================
        //the Proxy needs to send a request to the remote host and give back the data to client
        
        //connect to remote host
        int remoteFD = clientConnectToRemote(remoteHost.c_str(), REMOTE_SERVER_PORT);

        #ifdef DEBUG
        cout<<"remoteFD: "<<remoteFD<<endl;
        cout<<"remote connection port: "<<REMOTE_SERVER_PORT<<endl;
        #endif

        if (remoteFD < 0) {
            fprintf(stderr, "Cannot make a connection to a remote host %s on port %s\n", remoteHost.c_str(), REMOTE_SERVER_PORT);
            free(remoteReq);
            return -1;
        }
        
        //send the request to remote host
        if (send(remoteFD, remoteReq, remoteReqLength, 0) == -1) {
            perror("send");
            free(remoteReq);
            close(remoteFD);
            return -1;
        }
        #ifdef DEBUG
        cout<<"proxy send request to remote host"<<endl;
        #endif
        //get data from remote host into remoteResponse(string)
        if (getDatafromHost(remoteFD, remoteResponse) != 0 ) {
            free(remoteReq);
            close(remoteFD);
            return -1;
        }

        #ifdef DEBUG
        cout<<"get data from remote host"<<endl;
        // cout<<"response: "<<remoteResponse<<endl;
        #endif

        //Add to the cacheTable if Cache-Control is not private
        // if(strstr(remoteResponse.c_str(), "Cache-Control: private") == NULL)
        // {
            pthread_mutex_lock(mutex);
            cacheTable->insert(pair<string, string>(fullPath, remoteResponse));
            pthread_mutex_unlock(mutex);
        // }

        close(remoteFD);
    }
    
    //------------------Get the response and send to client----------------------------
    if(send(clientFD, remoteResponse.c_str(), remoteResponse.length(), 0) == -1)
    {
        perror("send");
        free(remoteReq);
        return 0;
    }
    
    #ifdef DEBUG
    cout<<"proxy send response to client"<<endl;
    #endif

    free(remoteReq);
    return 0;
}


void * pthread_ClientToProxy(void * params)
{
    PthreadPara *p=(PthreadPara*) params;
    clientToProxy(p->clientFD, p->cacheTable, p->mutex);
    
    close(p->clientFD);
    free(params);
    
    return NULL;
}
// #include <iostream>
// #include <string>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netdb.h>
// #include <netinet/in.h>
// // #include "http-common.h"

// using namespace std;

// int get_data(string hostname, string &result_buffer)
// {
//     struct addrinfo hints;
//     struct addrinfo *res;
//     char buf[256];
//     int byte_count;
    
//     memset(&hints,0,sizeof hints);
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
//     int status = getaddrinfo(hostname.c_str(), "http", &hints, &res );
//     if (status !=0)
//     {
//         fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
//         exit(1);
//     }
//     int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
//     connect(sockfd, res->ai_addr, res->ai_addrlen);
//     //char hostname[NI_MAXHOST] = "";
//     //int error = getnameinfo(res->ai_addr, res->ai_addrlen, hostname, NI_MAXHOST, NULL, 0,0);
//     //if (error !=0)
//     //{
//     //    fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(error));
//     //}
//     //if (hostname!='\0')
//     //    printf("hostname: %s\n", hostname);
//     while(true)
//     {
//         byte_count = recv(sockfd, buf, sizeof buf, 0);
//         cout<<byte_count<<endl;
//         if (byte_count<0)
//         {
//             perror("recv");
//             return -1;
//         }
//         else if (byte_count==0)
//         {
//             break;
//         }
//         result_buffer.append(buf,byte_count);
//     }
//     return 0;
// }
