/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/** @file http-proxy.cc
 *@author Xiao Li (pololee@cs.ucla.edu)  Jiamin Chen ()
 *@data 2013/4/26
 */

#include "http-proxy.h"
using namespace std;

#define DEBUG 1
#define MOREDEBUG 1

#ifdef DEBUG
#define TRACE(x) cout<<x<<endl
#define pthread_mutex_lock(x) pthread_mutex_lock(x); std::cout<<"lock "<<x<<endl;
#define pthread_mutex_unlock(x) pthread_mutex_unlock(x); std::cout<<"unlock "<<x<<endl;
#else
#define TRACE(X)
#endif

//---------------------------Declaration of common variable-------------------------
Cache cache;
const int MAXIMUM_PROCESS = 10;
pthread_mutex_t count_mutex;
pthread_cond_t count_threshold;
pthread_mutex_t cache_mutex;
int threadCount;
#define PROXY_SERVER_PORT 14805
#define REMOTE_SERVER_PORT 80
#define BACKLOG 100
#define BUFSIZE 2048
#define PROXY_LISTEN_IP "127.0.0.1"


//---------------------------Declaration of Functions-----------------------------------
//@brief:Similar to the example in Beej's Guide to Networking Programming
//      return the listening Socket
int iniServerListen();

//@Brief: deal with communication between client and proxy
//      Get the request from client
//      Parse the request
//      Give back response to client(cache and new request)
void* clientToProxy(void * sock);

//@brief: Get Response
string getResponse(HttpRequest req);

//@brief: 
string fetchResponse(HttpRequest req);

//@brief: Make connection to Remote
//      return the socket
int proxyConnectToRemote(const char *ip, const unsigned short port);

//@brief: Get Data from the remote host
string getDatafromHost(int remoteFD, HttpResponse* response);


/**
 * @brief convert hostname to ip address
 * @TODO if cannot resolve host, what will happen?
 *
 * @param hostname
 * @return ip address
 */
char * get_ip(const char * host);

//@brief:Convert a string to Time
time_t convertTime(string s);

//@brief: Create a Error msg
HttpResponse createErrorMsg(string reason);

//@brief: Create a TCP socket
int createTCPSocket();





int main (int argc, char *argv[])
{
    //Initial global variable
    threadCount = 0;
    pthread_mutex_init(&count_mutex, NULL);
    pthread_cond_init(&count_threshold, NULL);
    pthread_mutex_init(&cache_mutex, NULL);
    
    //Create proxy server and make it listen
    int proxySockFD = iniServerListen();
    if (proxySockFD < 0) {
        fprintf(stderr, "Cannot make proxy server listen on port %d\n", PROXY_SERVER_PORT);
        return 1;
    }
    
    
    //Loop to accept the request
    while (true) {
        //when more than MAXIMUM_PROCESS clients ask request,
        //suspend until someone finish
        pthread_mutex_lock(&count_mutex);
//        if(threadCount >= MAXIMUM_PROCESS) {
//            pthread_cond_wait(&count_threshold, &count_mutex);
//        }
        pthread_mutex_unlock(&count_mutex);
        
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        
        //Accept connection
        int* clientFD = new int;
        *clientFD = accept(proxySockFD, (struct sockaddr *)&cli_addr, &len);
        if (*clientFD == -1) {
            perror("accept");
            continue;
        }
        
        int yes = 1;
        int opt_status = setsockopt(*clientFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (opt_status == -1)
        {
            perror("server: setsockopt");
            exit(1);
        }
        
#ifdef MOREDEBUG
        cout<<"New Client ";
        cout<<"clientFD: "<<*clientFD<<endl;
//        inet_ntop(their_addr.ss_family, getIPAddr((struct sockaddr *)&their_addr), ipAddr, sizeof(ipAddr));
//        printf("Proxy: got connection from %s\n", ipAddr);
#endif
        
        //Create thread
        TRACE("COUNT_MUTEX");
        pthread_mutex_lock(&count_mutex);
        threadCount++;
        TRACE("CREATE THREAD "<<threadCount);
        TRACE("COUNT_MUTEX");
        pthread_mutex_unlock(&count_mutex);
        
        pthread_t clientThread;
        pthread_create(&clientThread, NULL, clientToProxy, (void *)clientFD);
    }
    close(proxySockFD);
    return 0;
}





int iniServerListen()
{
	int sock_listen = createTCPSocket();
    int yes = 1;
    int opt_status = setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (opt_status == -1)
    {
        perror("server: setsockopt");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    inet_aton(PROXY_LISTEN_IP, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(PROXY_SERVER_PORT);
    if((bind(sock_listen, (struct sockaddr*)&serv_addr, sizeof(serv_addr)))<0){
        cerr<<"ERROR on binding"<<endl;
        exit(1);
    }
    TRACE("bind successful");
    if(listen(sock_listen,BACKLOG)<0){
        cerr<<"ERROR on listening"<<endl;
        exit(1);
    }
    TRACE("listen succesful");
    
#ifdef MOREDEBUG
    printf("proxy Port: %d\n", PROXY_SERVER_PORT);
#endif

#ifdef MOREDEBUG
	cout<<"Start listening, waiting for connections..."<<endl;
    cout<<"-----------------------------------------------------------------------"<<endl;
#endif
    return sock_listen;
}





void* clientToProxy(void * sock)
{
    //If there is no request beyond 60 seconds, the proxy will close the connection to client
    int clientFD = *((int*)sock);
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(clientFD, &fds);
    
    int rc;
    while ((rc=select(clientFD+1, &fds, NULL, NULL, &timeout)) != 0)
    {
        TRACE("TIMER "<<timeout.tv_sec<<"+"<<timeout.tv_usec);
        TRACE("select rc: "<<rc);
        if (rc < 0)
        {
            perror("select failure");
            break;
        }
        
        if (rc == 0) {
            TRACE("Time Out");
            break;
        }
        string clientBuffer;
        char bufTemp[BUFSIZE];
        try {
            ssize_t size_recv;
            while((size_recv = recv(clientFD, bufTemp, BUFSIZE,0)) > 0){
//                TRACE("message received is "<<bufTemp);
                //TRACE("size recieved is "<<size_recv);
                
                clientBuffer.append(bufTemp,size_recv);
                
                if((clientBuffer.find("\r\n\r\n"))!=string::npos){
                    break;
                }
            }
            TRACE("request recv size: "<<size_recv);
            if(size_recv == 0){ //the other side close the connection
                TRACE("the other side close connection "<<clientFD);
                    break;
            }
           
            if(size_recv<0){
                throw ParseException("400/Bad Request");
            }            //Parse request from client
            
            
            HttpRequest clientReq;
            clientReq.ParseRequest(clientBuffer.c_str(), clientBuffer.length());
            
#ifdef MOREDEBUG
            cout<<"******************Request***********************"<<endl;
            TRACE("clientFD"<<clientFD);
            unsigned long headerTail = clientBuffer.find("\r\n\r\n", 0);
            cout<<clientBuffer.substr(0, headerTail)<<endl;
            cout<<"***********************************************"<<endl;
#endif
            //Get Response
            const char * data;
            try {
                string rsp = getResponse(clientReq);
                data = rsp.c_str();
                cout<<"@@DATA received, forward to the client clientFD "<<clientFD<<endl;
                send(clientFD, data, strlen(data), 0);
            }
            catch (ParseException ex) {
                TRACE("Parse Exception for RESPONSE");
                string reason = "502/Bad Gateway";
                HttpResponse rspToClient = createErrorMsg(reason);
                char resPond[rspToClient.GetTotalLength()];
                rspToClient.FormatResponse(resPond);
                send(clientFD, resPond, strlen(resPond), 0);
            }
            catch(HttpException ex){
                HttpResponse resp = createErrorMsg(ex.what());
                char respD[resp.GetTotalLength()];
                resp.FormatResponse(respD);
                send(clientFD, respD, strlen(respD), 0);
            }
        }
        catch(ParseException ex){ // Invalid Request throw ParseException
            TRACE("Parse Exeption for REQUEST");
#ifdef DEBUG
            printf("Exception raised: %s\n", ex.what());
#endif
            string cmp = "Request is not GET";
            string reason;
            if (strcmp(ex.what(), cmp.c_str()) != 0) {
                reason = " 400/Bad Request";
            }
            else{
                reason = " 501/Not Implemented";
            }
            HttpResponse rspToClient = createErrorMsg(reason);
            char resPond[rspToClient.GetTotalLength()];
            rspToClient.FormatResponse(resPond);
            send(clientFD, resPond, strlen(resPond), 0);
        }
        catch(exception ex){
            TRACE("unexpexted exception "<<ex.what());
        }
    }

#ifdef MOREDEBUG
    cout<<"!!!finish everything, will close fd and exit thread clientFD "<<clientFD<<endl<<endl;
#endif
    
    close(clientFD);
    TRACE("close client socket "<<clientFD);
    pthread_mutex_lock(&count_mutex);
    threadCount--;
//    if (threadCount < MAXIMUM_PROCESS) {
//            pthread_cond_signal(&count_threshold);
//    }
    pthread_mutex_unlock(&count_mutex);
    TRACE("After kill THREAD "<<threadCount);
    pthread_exit(NULL);
}




string getResponse(HttpRequest req)
{
    TRACE("~~~~~~~~~~~~~Response~~~~~~~~~~~~~~");
    //Get the url
    ostringstream ss;
    ss<< req.GetHost()<<":"<<req.GetPort()<<req.GetPath();
    string url = ss.str();
    
    //Try to get it from cache
    cout<<"url is "<<url<<" ";
    TRACE("try to get from cache");
    
    TRACE("CASHE_MUTEX");
    pthread_mutex_lock(&cache_mutex);
    
    Webpage* pg = cache.get(url);
    if(pg!=NULL){
        TRACE("webpage in cache we get is "<<pg->getExpire());
        if (!pg->isExpired()){
            TRACE("webpage in cahce, not expired");
            string d = pg->getData();
            
            TRACE("CASHE_MUTEX");
            pthread_mutex_unlock(&cache_mutex);
            return d;
        }
        else{
            if(pg->getLastModify() !=""){
                TRACE("try to use last modify");
                req.AddHeader("If-Modified-Since", pg->getLastModify());
            }
            TRACE("CASHE_MUTEX");
            pthread_mutex_unlock(&cache_mutex);
            return fetchResponse(req);
        }
    }
    else{
        TRACE("CASHE_MUTEX");
        pthread_mutex_unlock(&cache_mutex);
        TRACE("directly fetch");
        return fetchResponse(req);
    }
}


string fetchResponse(HttpRequest req){
    //Get the URL
    ostringstream ss;
    ss<< req.GetHost()<<":"<<req.GetPort()<<req.GetPath();
    string url = ss.str();
    TRACE("url is "<<url);
    
    //PROXY connect to REMOTE HOST
    const char* host = (req.GetHost()).c_str();
    unsigned short port = htons(req.GetPort());
    char* ip = get_ip(host);
    int sockFetch = proxyConnectToRemote(ip, port);
    if(sockFetch<0){
      	throw HttpException("502", "Bad Gateway");
    }
    TRACE("Connection established");
    
#ifdef MOREDEBUG
    cout<<"~~~Get data from server remoteFD "<<sockFetch<<endl;
#endif
    
    //construct request and send it to remote host
    size_t size_req = req.GetTotalLength();
    char buf_req[size_req];
    bzero(buf_req, size_req);
    req.FormatRequest(buf_req);
    if(send(sockFetch, buf_req, size_req, 0)<0){
      	cerr<<"send failed when fetching data from remote server"<<endl;
        throw HttpException("502", "Bad Gateway");
    }
    TRACE("Message sent to the remote server");
    
    //Get the response
    TRACE("Get the response");
    HttpResponse resp;
    string data = getDatafromHost(sockFetch, &resp);
    string statusCode = resp.GetStatusCode();
    TRACE("status code is "<<statusCode);
    if (statusCode=="200") {
        //Normal data package
        string expire = resp.FindHeader("Expires");
        string date = resp.FindHeader("Date");
        string lastModi = resp.FindHeader("Last-Modified");
        TRACE("expire as "<<expire<<"\ndate as "<<date<<"\nlastModi as "<<lastModi);
        
	    time_t expire_t;
        time_t now = time(NULL);
        TRACE("try to add to cache");
        if (expire != "") {
            if((expire_t = convertTime(expire))!=0 && difftime(expire_t, now)>0){
                TRACE("add to cache with normal expire");
                Webpage pg(expire_t, lastModi, data);
                TRACE("CASHE_MUTEX");
                pthread_mutex_lock(&cache_mutex);
                cache.add(url, pg);
                TRACE("CASHE_MUTEX");
                pthread_mutex_unlock(&cache_mutex);
            }
            else{
                TRACE("expire exists but not valid");
                pthread_mutex_lock(&cache_mutex);
                cache.remove(url);
                pthread_mutex_unlock(&cache_mutex);
            }
        }
    }
    else if(statusCode == "304"){ //content not changed
        string expire = resp.FindHeader("Expires");
        time_t newExpire = convertTime(expire);
        
        pthread_mutex_lock(&cache_mutex);
        cache.get(url)->ModifyExpire(newExpire);
        data = cache.get(url)->getData();
        pthread_mutex_unlock(&cache_mutex);
        TRACE(304);
    }
    TRACE("finish adding cache");

    
    string clientAskClose = req.FindHeader("Connection");
    cout<<"client ask close "<<clientAskClose<<endl;
    string serverAskClose = resp.FindHeader("Connection");
    cout<<"server ask close "<<serverAskClose<<endl;
    string cmp = "close";
    bool whetherClose = false;
    if ((strcmp(clientAskClose.c_str(), cmp.c_str()) != 0) || (strcmp(serverAskClose.c_str(), cmp.c_str()) != 0) ) {
        whetherClose = true;
    }
    
    if(whetherClose){
        close(sockFetch);
        #ifdef MOREDEBUG
            cout<<"&&&Finish Fetching Data close remoteFD "<<sockFetch<<endl;
        #endif
    }
    return data;
}




int proxyConnectToRemote(const char *ip, const unsigned short port)
{
    int sock_fetch = createTCPSocket();
    int yes = 1;
    int opt_status = setsockopt(sock_fetch, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (opt_status == -1)
    {
        perror("server: setsockopt");
        exit(1);
    }
    
    struct sockaddr_in client;
    client.sin_family = AF_INET;
    inet_aton(ip, &client.sin_addr);
    client.sin_addr.s_addr = inet_addr(ip);
    client.sin_port = port;
    if(connect(sock_fetch, (struct sockaddr*)&client, sizeof(client))<0){
      	throw HttpException("502", "Bad Gateway");
    }
    
    return sock_fetch;
}




string getDatafromHost(int remoteFD, HttpResponse* response)
{
    bool isHeader = true;
    bool isChunk = false;
    
    unsigned long headerHead = string::npos;
    unsigned long headerTail = string::npos;
    
    ssize_t recvSize;
    string recvBuf;
    char bufTemp[BUFSIZE];
    
    long contentLeft = 0;
    while ((recvSize = recv(remoteFD, bufTemp, BUFSIZE, 0)) > 0) {
        TRACE("Rece from remote recvsize: "<<recvSize);
        recvBuf.append(bufTemp, recvSize);
        
        string body;
        if (isHeader) {
            //To check if header has been parsed
            if (headerHead == string::npos) {
                headerHead = recvBuf.find("HTTP/");
                if (headerHead == string::npos) {
                    throw ParseException("Incorrectly formatted RESPONSE");
                }
            }
            
            if ((headerTail = recvBuf.find("\r\n\r\n", headerHead)) != string::npos) {
                //finish receiving the HTTP header part
                string header = recvBuf.substr(headerHead, headerTail + 4 - headerHead);
                TRACE("header is: \n"<<header);
                body = recvBuf.substr(headerTail + sizeof("\r\n\r\n") - 1);
                TRACE("body is: \n"<<body<<"\n\n");
                
                response->ParseResponse(header.c_str(), header.length());
                string contentLength = response->FindHeader("Content-Length");
                TRACE("Content-Length is <"<<contentLength<<">");
                
                //Given content length, we can check if it is transfer encoding, i.e. chunked responsed data
                if (contentLength != "") {
                    contentLeft = atol(contentLength.c_str());
                    isChunk = false;
                    contentLeft -= body.size();
                }
                else{
                    TRACE("Find Transfer-Encoding "<<response->FindHeader("Transfer-Encoding"));
                    if (response->FindHeader("Transfer-Encoding") == "chunked") {
                        TRACE("chunked");
                        isChunk = true;
                        if (body.find("0\r\n\r\n") != string::npos) {
                            break;
                        }
                    }
                    else{
                        throw ParseException("Incorrectly formatted RESPONSE (Transfer-Encoding)");
                    }
                }
                
                TRACE("isChunk: "<<isChunk);
                isHeader = false;
            }
        }
        else{
            //check whether the message has ended
            if (isChunk) {
                //For chunked case, we use "0\r\n\r\n" to determine the end of the response data
                body = bufTemp;
                if(body.find("0\r\n\r\n") != string::npos){
                    break;
                }
            }
            else{
                //For the normal case, we use content-length to determin the end
                contentLeft -= recvSize;
                TRACE("Content-Left is <"<<contentLeft<<">");
                if (contentLeft <= 0) {
                    break;
                }
            }
        }//not header part
        bzero(bufTemp, BUFSIZE);
    }//while for reading data
    return recvBuf;
}



char * get_ip(const char * host){
	struct hostent *hent;
	int iplen = 15;
	char *ip = (char*) malloc(iplen+1);
	memset(ip,0,iplen+1);
	if((hent = gethostbyname(host))==NULL){
		throw HttpException("404","Not found");
	}
	if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0],ip,iplen)==NULL){
		throw HttpException("503","Bad Gateway");
	}
	return ip;
}



time_t convertTime(string ts)
{
    const char* format = "%a, %d %b %Y %H:%M:%S %Z";
    struct tm tm;
    if(strptime(ts.c_str(), format, &tm)==NULL){
    	return 0;
    }
    else{
    	tm.tm_hour = tm.tm_hour-8;//to la time
    	return mktime(&tm);
    }
}




HttpResponse createErrorMsg(string reason){
    HttpResponse resp;
    resp.SetVersion("1.1");
    size_t split = reason.find('/');
    string code = reason.substr(0,split);
    string msg = reason.substr(split+1);
    TRACE("code is "<<code<<" msg is "<<msg);
    resp.SetStatusCode(code);
    resp.SetStatusMsg(msg);
    return resp;
}



int createTCPSocket(){
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0){
        cerr<<"ERROR on accept"<<endl;
        exit(1);
    }
    return sock;
}