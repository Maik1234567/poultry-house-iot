#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <map>
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define HEX 16
extern unsigned long g_millis; extern std::map<int,int> g_pins, g_pwm, g_adc;
inline unsigned long millis(){ return g_millis; }
inline void delay(unsigned long ms){ g_millis += ms; }
inline void pinMode(int,int){}
inline void digitalWrite(int p,int v){ g_pins[p]=v; }
inline int analogRead(int p){ return g_adc[p]; }
#ifdef CORE3
#define ESP_ARDUINO_VERSION_MAJOR 3
inline void ledcAttach(int,int,int){}
inline void ledcWrite(int pin,int d){ g_pwm[pin]=d; }
#else
#define ESP_ARDUINO_VERSION_MAJOR 2
inline void ledcSetup(int,int,int){}
inline void ledcAttachPin(int,int){}
inline void ledcWrite(int ch,int d){ g_pwm[100+ch]=d; }
#endif
struct String { std::string s; String(const char* c):s(c){} String(uint32_t v,int){ char b[16]; snprintf(b,16,"%x",v); s=b;}
  String operator+(const String& o) const { String r(""); r.s=s+o.s; return r;} const char* c_str() const {return s.c_str();} };
inline String operator+(const char* a,const String& b){ return String(a)+b; }
struct SerialT { void begin(int){} template<class...A> void printf(const char* f,A...a){ ::printf(f,a...);} void print(const char* s){::printf("%s",s);} void print(char c){::putchar(c);} void println(const char* s){::printf("%s\n",s);} };
extern SerialT Serial;
struct EspT { uint64_t getEfuseMac(){ return 0x1234; } }; extern EspT ESP;
