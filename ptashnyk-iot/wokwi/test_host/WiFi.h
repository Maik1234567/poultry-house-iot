#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
struct WiFiT { void begin(const char*,const char*,int){} int status(){return WL_CONNECTED;} }; extern WiFiT WiFi;
struct WiFiClient {};
