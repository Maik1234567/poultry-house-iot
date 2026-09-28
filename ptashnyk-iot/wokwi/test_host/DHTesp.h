#pragma once
struct TempAndHumidity { float temperature, humidity; };
extern float g_tin, g_hin, g_tout;
struct DHTesp { enum Model{DHT22}; enum Status{ERROR_NONE}; int pin=0;
  void setup(int p, Model){ pin=p; } TempAndHumidity getTempAndHumidity(){ return pin==4 ? TempAndHumidity{g_tin,g_hin} : TempAndHumidity{g_tout,80}; }
  Status getStatus(){ return ERROR_NONE; } };
