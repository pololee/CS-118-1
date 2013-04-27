/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/** @file http-proxy.cc
*@author Xiao Li (pololee@cs.ucla.edu)
*@data 2013/4/26
*/

#include <iostream>
#include "http-common.h"
using namespace std;

int main (int argc, char *argv[])
{
  // command line parsing
  cout<<"hello, world!"<<endl;
  int proxySockFD = iniServerListen(PROXY_SERVER_PORT);
	cout<<"finish listen\n";
	cout<<"proxySockFD: "<<proxySockFD<<endl;
  return 0;
}
