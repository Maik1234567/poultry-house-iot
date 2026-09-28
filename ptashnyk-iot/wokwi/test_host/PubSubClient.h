#pragma once
#include "Arduino.h"
#include <vector>
#include <functional>
struct Pub { std::string t,m; bool r; };
extern std::vector<Pub> g_pubs;
struct PubSubClient { bool c=false; void(*cb)(char*,byte*,unsigned int)=nullptr;
  PubSubClient(WiFiClient&){} void setServer(const char*,int){} void setBufferSize(int){} void setCallback(void(*f)(char*,byte*,unsigned int)){cb=f;}
  bool connected(){return c;} bool connect(const char*,const char* wt,int,bool,const char* wm){ c=true; g_pubs.push_back({std::string("WILL ")+wt,wm,true}); return true;}
  bool publish(const char* t,const char* m,bool r=false){ g_pubs.push_back({t,m,r}); return true;}
  bool subscribe(const char* t,int){ g_pubs.push_back({std::string("SUB ")+t,"",false}); return true;} void loop(){} int state(){return 0;} };
