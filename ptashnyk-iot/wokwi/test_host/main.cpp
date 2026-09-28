// Перевірка скетча на ПК: заглушки Arduino/WiFi/MQTT/DHT, прогін loop() і перевірка реакції виходів.
#include "Arduino.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include "DHTesp.h"
unsigned long g_millis=0; std::map<int,int> g_pins,g_pwm,g_adc; std::vector<Pub> g_pubs;
float g_tin=23, g_hin=60, g_tout=-5; SerialT Serial; EspT ESP; WiFiT WiFi;
#include "../sketch.ino"
#include <cassert>
static void run(unsigned long ms){ unsigned long end=g_millis+ms; while(g_millis<end){ loop(); g_millis+=10; } }
int main(){
  g_adc[34]=819; g_adc[35]=1066;   // ~10 ppm NH3, ~1600 ppm CO2
  setup(); run(10000);
  int fan_base = g_pwm.count(26)?g_pwm[26]:g_pwm[101];
  // 1) аміак зростає до 18 ppm -> вентиляція росте
  g_adc[34]=(int)(18*4095/50.0); run(10000);
  int fan_hi = g_pwm.count(26)?g_pwm[26]:g_pwm[101];
  printf("\nвентилятор: при 10 ppm duty=%d, при 18 ppm duty=%d\n", fan_base, fan_hi);
  assert(fan_hi > fan_base);
  // 2) холодно -> нагрів на максимум і реле увімкнене
  g_tin=19; run(20000);
  int heat = g_pwm.count(25)?g_pwm[25]:g_pwm[100];
  printf("нагрів при 19 °C: duty=%d, реле=%d\n", heat, g_pins[23]); fflush(stdout); assert(heat>200 && g_pins[23]==1);
  // 3) NH3 > 20 -> режим ALARM, жовтий LED, повідомлення в alarm
  g_adc[34]=(int)(30*4095/50.0); run(6000);
  bool alarm=false; for(auto&p:g_pubs) if(p.t=="ir45-maik/farm/h1/alarm") alarm=true;
  printf("тривога: LED=%d, повідомлення=%d\n", g_pins[27], alarm); assert(g_pins[27]==1 && alarm);
  // 4) уставка через MQTT
  char t[]="ir45-maik/farm/h1/setpoints"; const char* ok="{\"temp_setpoint\": 24.5}"; const char* bad="{\"temp_setpoint\": 60}";
  onMessage(t,(byte*)ok,strlen(ok)); assert(ctrl.setpoint()==24.5f);
  onMessage(t,(byte*)bad,strlen(bad)); assert(ctrl.setpoint()==24.5f);
  // 5) LWT і підписки
  assert(g_pubs[0].t=="WILL ir45-maik/farm/h1/zoneA/status" && g_pubs[0].m=="offline");
  for(auto&p:g_pubs) if(p.t.rfind("ir45-maik/farm/h1/zoneA/telemetry",0)==0){ printf("приклад телеметрії: %s\n", p.m.c_str()); break; }
  printf("OK: усі перевірки скетча пройдено\n");
}
