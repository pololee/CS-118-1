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

//boost library
#include <boost/lexical_cast.hpp>

//skeleton code library
#include "http-request.h"
#include "http-response.h"
#include "compat.h"

using namespace std;

class Webpage{
public:
    Webpage(time_t expire, string lModify, string dt){
        expireTime = expire;
        lastModify = lModify;
        data = dt;
    }
    
    time_t getExpire(void){
        return expireTime;
    }
    
    string getData(void){
        return data;
    }
    
    bool isExpired(){
        time_t now = time(NULL);
        
        cout<<"DIFF TIME "<<difftime(expireTime, now)<<endl
                <<"NOW "<<now<<endl
                <<"EXPIRE "<<expireTime<<endl;
        if(difftime(expireTime, now)<0){
            return true;
        }
        else{
            return false;
        }
    }
    
    string getLastModify(){
        return lastModify;
    }
    
    void ModifyExpire(const time_t &newExpire){
        expireTime = newExpire;
    }
    
private:
    time_t expireTime;
    string lastModify;
    string data;
};


class Cache{
public:
    Webpage* get(string url){
        map<string,Webpage>::iterator it;
        it = storage.find(url);
        if(it == storage.end()){
            return NULL;
        }
        else{
            return &it->second;
        }
    }
    
    void add(string url, Webpage pg){
        storage.erase(url);
        storage.insert(map<string, Webpage>::value_type(url, pg));
    }
    
    void remove(string url){
        storage.erase(url);
    }
    
private:
    map<string, Webpage> storage;
} ;


class HttpException : public std::exception
{
public:
    HttpException (const std::string &code, const std::string &reason) : m_reason (code+"/"+reason) { }
    virtual ~HttpException () throw () { }
    virtual const char* what() const throw ()
    { return m_reason.c_str (); }
private:
    std::string m_reason;
};