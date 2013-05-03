/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/** @file http-proxy.cc
 *@author Xiao Li (pololee@cs.ucla.edu)  Jiamin Chen ()
 *@data 2013/4/26
 */

#include "http-proxy.h"
using namespace std;

#define DEBUG 1

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
#define PROXY_SERVER_PORT "48670"
#define REMOTE_SERVER_PORT "80"
#define BACKLOG 100
#define BUFSIZE 2048


//---------------------------Declaration of Functions-----------------------------------
//@brief:Similar to the example in Beej's Guide to Networking Programming
//      return the listening Socket
int iniServerListen(const char *port);

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
int proxyConnectToRemote(const char *host,const char *port);

//@brief: Get Data from the remote host
string getDatafromHost(int remoteFD, HttpResponse* response);


//@brief: Get the IP address
void *getIPAddr(struct sockaddr *sa);

//@brief:Convert a string to Time
time_t convertTime(string s);

//@brief: Create a Error msg
HttpResponse createErrorMsg(string reason);





int main (int argc, char *argv[])
{
    //Initial global variable
    threadCount = 0;
    pthread_mutex_init(&count_mutex, NULL);
    pthread_cond_init(&count_threshold, NULL);
    pthread_mutex_init(&cache_mutex, NULL);
    
    //Create proxy server and make it listen
    int proxySockFD = iniServerListen(PROXY_SERVER_PORT);
    if (proxySockFD < 0) {
        fprintf(stderr, "Cannot make proxy server listen on port %s\n", PROXY_SERVER_PORT);
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
        
        struct sockaddr_storage their_addr;
        char ipAddr[INET6_ADDRSTRLEN];
        socklen_t sin_size = sizeof(their_addr);
        
        //Accept connection
        int clientFD = accept(proxySockFD, (struct sockaddr *)&their_addr, &sin_size);
        if (clientFD == -1) {
            perror("accept");
            continue;
        }
        
#ifdef DEBUG
        cout<<"++++++++++New Client++++++++++++++++++"<<endl;
        cout<<"clientFD: "<<clientFD<<endl;
        inet_ntop(their_addr.ss_family, getIPAddr((struct sockaddr *)&their_addr), ipAddr, sizeof(ipAddr));
        printf("Proxy: got connection from %s\n", ipAddr);
#endif
        
        //Create thread
        TRACE("COUNT_MUTEX");
        pthread_mutex_lock(&count_mutex);
        threadCount++;
        TRACE("CREATE THREAD "<<threadCount);
        TRACE("COUNT_MUTEX");
        pthread_mutex_unlock(&count_mutex);
        
        pthread_t clientThread;
        pthread_create(&clientThread, NULL, clientToProxy, (void *)&clientFD);
    }
    close(proxySockFD);
    return 0;
}





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
        if (sock_fd == -1)
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
        if (bind_status == -1)
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
    
#ifdef DEBUG
    printf("proxy Port: %s\n", port);
    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, getIPAddr((struct sockaddr *)p->ai_addr), s, sizeof s);
    fprintf(stderr, "listener bind to %s\n", s);
#endif
    
    // Don't need the structure with address info any more
    freeaddrinfo(res);
    
    // Start listening
    // BACKLOG: the number of connections allowed on the incoming queue
    if (listen(sock_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
#ifdef DEBUG
	cout<<"Start listening, waiting for connections..."<<endl;
    cout<<"-----------------------------------------------------------------------"<<endl;
#endif
    return sock_fd;
}





void* clientToProxy(void * sock)
{
    //If there is no request beyond 60 seconds, the proxy will close the connection to client
    int clientFD = *((int*)sock);
    struct timeval timeout;
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(clientFD, &fds);
    
    int rc;
    while ((rc=select(clientFD+1, &fds, NULL, NULL, &timeout)) != 0)
    {
        TRACE("TIMEOUT "<<timeout.tv_sec<<"+"<<timeout.tv_usec);
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
                //TRACE("size recieved is "<<size_recv)
                
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
            TRACE("******************Request***********************");
            TRACE("clientFD"<<clientFD);
            unsigned long headerTail = clientBuffer.find("\r\n\r\n", 0);
            TRACE(clientBuffer.substr(0, headerTail));
            TRACE("******************Request***********************");
            
            
            //Get Response
            const char * data;
            try {
                string rsp = getResponse(clientReq);
                data = rsp.c_str();
                TRACE("DATA received, forward to the client");
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
    TRACE("++++++++++++++Close Client++++++++++++++++");
}




string getResponse(HttpRequest req)
{
    TRACE("~~~~~~~~~~~~~Response~~~~~~~~~~~~~~");
    //Get the url
    ostringstream ss;
    ss<< req.GetHost()<<":"<<req.GetPort()<<req.GetPath();
    string url = ss.str();
    
    //Try to get it from cache
    TRACE("url is "<<url);
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
    const string port = boost::lexical_cast<string> (req.GetPort());
    int sockFetch = proxyConnectToRemote(host, port.c_str());
    if(sockFetch<0){
      	throw HttpException("502", "Bad Gateway");
    }
    TRACE("Connection established");
    
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
    close(sockFetch);
    return data;
}




int proxyConnectToRemote(const char *host,const char *port)
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
    // fprintf(stderr, "client: connecting to %s\n", s);
#ifdef DEBUG
    printf("proxy: connecting to %s\n", s);
#endif
    // Don't need the structure with address info any more
    freeaddrinfo(res);
    
    return sock_fd;
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



void *getIPAddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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