// Вузол керування мікрокліматом пташника на ESP32 (проєкт для симулятора Wokwi).
//
// Використовує те саме ядро алгоритму, що й firmware/core (climate_controller.*).
// Заміни давачів у симуляторі (у Wokwi немає SHT31, SCD30, MQ-137):
//   DHT22 #1 (D4)  – температура/вологість у приміщенні   (замість SHT31)
//   DHT22 #2 (D15) – зовнішня температура
//   потенціометр NH3 (D34) – 0..50 ppm                    (замість MQ-137)
//   потенціометр CO2 (D35) – 400..5000 ppm                (замість SCD30)
// Виконавчі пристрої:
//   червоний LED (D25, ШІМ) – потужність нагріву 0..100 %
//   синій LED   (D26, ШІМ) – продуктивність вентиляції V_min..V_max
//   реле        (D23)      – контактор нагрівача (увімкнено, якщо Q > 5 %)
//   жовтий LED  (D27)      – тривога (режим ALARM)
// Зв'язок: Wi-Fi «Wokwi-GUEST», MQTT-брокер broker.hivemq.com:1883.

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include "climate_controller.h"

// ---------- налаштування ----------
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 1883;
// Унікальний префікс, бо публічний брокер спільний для всіх
const char* BASE = "ir45-maik/farm/h1";
const char* ZONE = "zoneA";

const int PIN_DHT_IN = 4, PIN_DHT_OUT = 15;
const int PIN_NH3 = 34, PIN_CO2 = 35;
const int PIN_HEAT_LED = 25, PIN_FAN_LED = 26, PIN_ALARM_LED = 27, PIN_RELAY = 23;

const unsigned long CONTROL_MS = 1000;    // період алгоритму, 1 с (розд. 3.3)
const unsigned long SENSOR_MS = 2000;     // DHT22 можна читати не частіше ніж раз на 2 с
const unsigned long PUBLISH_MS = 5000;    // у звіті 30 с; для демонстрації – 5 с

// ---------- об'єкти ----------
DHTesp dhtIn, dhtOut;
WiFiClient net;
PubSubClient mqtt(net);
climate::AdaptiveController ctrl;           // параметри за замовчуванням = табл. 3 звіту
climate::Measurements meas = {23.0, -5.0, 10.0, 2000.0};
climate::Output out = {0.9, 0.0, climate::Mode::Normal};
float humIn = 60.0;
unsigned long tControl = 0, tSensor = 0, tPublish = 0;
char topicTelemetry[64], topicStatus[64], topicAlarm[64], topicSetpoints[64], topicCmd[64];

// ---------- ШІМ: сумісно з Arduino-ESP32 2.x і 3.x ----------
void pwmInit(int pin, int ch) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)ch; ledcAttach(pin, 5000, 8);
#else
  ledcSetup(ch, 5000, 8); ledcAttachPin(pin, ch);
#endif
}
void pwmWrite(int pin, int ch, int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)ch; ledcWrite(pin, duty);
#else
  (void)pin; ledcWrite(ch, duty);
#endif
}

// ---------- вхідні повідомлення: уставка {"temp_setpoint": 24.5} ----------
bool parseSetpoint(const char* json, float* value) {
  const char* key = strstr(json, "\"temp_setpoint\"");
  if (!key) return false;
  const char* colon = strchr(key, ':');
  if (!colon) return false;
  char* end = nullptr;
  float v = strtof(colon + 1, &end);
  if (end == colon + 1) return false;
  *value = v;
  return true;
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  char buf[128];
  unsigned int n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
  memcpy(buf, payload, n); buf[n] = 0;
  float sp;
  if (parseSetpoint(buf, &sp) && sp >= 15.0f && sp <= 35.0f) {
    ctrl.setSetpoint(sp);
    Serial.printf("[MQTT] %s -> нова уставка %.1f °C\n", topic, sp);
  } else {
    Serial.printf("[MQTT] %s -> відхилено: %s\n", topic, buf);
  }
}

void connectWiFi() {
  Serial.print("Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print('.'); }
  Serial.println(" OK");
}

void connectMqtt() {
  while (!mqtt.connected()) {
    String id = "ir45-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    // LWT: якщо вузол зникне, брокер сам опублікує "offline"
    if (mqtt.connect(id.c_str(), topicStatus, 1, true, "offline")) {
      mqtt.publish(topicStatus, "online", true);
      mqtt.subscribe(topicSetpoints, 1);
      mqtt.subscribe(topicCmd, 1);
      Serial.println("MQTT OK");
    } else {
      Serial.printf("MQTT помилка %d, повтор через 2 с\n", mqtt.state());
      delay(2000);
    }
  }
}

void readSensors() {
  TempAndHumidity a = dhtIn.getTempAndHumidity();
  if (dhtIn.getStatus() == DHTesp::ERROR_NONE) { meas.T = a.temperature; humIn = a.humidity; }
  TempAndHumidity b = dhtOut.getTempAndHumidity();
  if (dhtOut.getStatus() == DHTesp::ERROR_NONE) meas.T_out = b.temperature;
  // 12-розрядний АЦП: 0..4095
  meas.nh3 = analogRead(PIN_NH3) * 50.0 / 4095.0;
  meas.co2 = 400.0 + analogRead(PIN_CO2) * 4600.0 / 4095.0;
}

void applyOutputs() {
  const climate::Params& p = ctrl.params();
  int heatDuty = (int)(255.0 * out.q_heat / p.q_heater_max + 0.5);
  int fanDuty = (int)(255.0 * (out.vent - p.v_min) / (p.v_max - p.v_min) + 0.5);
  pwmWrite(PIN_HEAT_LED, 0, heatDuty);
  pwmWrite(PIN_FAN_LED, 1, fanDuty < 10 ? 10 : fanDuty);   // V_min > 0: світиться завжди
  digitalWrite(PIN_RELAY, out.q_heat > 0.05 * p.q_heater_max ? HIGH : LOW);
  digitalWrite(PIN_ALARM_LED, out.mode == climate::Mode::Alarm ? HIGH : LOW);
}

void publishTelemetry() {
  char msg[320];
  snprintf(msg, sizeof(msg),
    "{\"ts\":%lu,\"temp\":%.2f,\"hum\":%.1f,\"nh3\":%.2f,\"co2\":%.0f,\"t_out\":%.1f,"
    "\"setpoint\":%.1f,\"vent_m3s\":%.3f,\"heater_w\":%.0f,\"mode\":\"%s\"}",
    millis() / 1000, meas.T, humIn, meas.nh3, meas.co2, meas.T_out,
    ctrl.setpoint(), out.vent, out.q_heat, climate::modeName(out.mode));
  mqtt.publish(topicTelemetry, msg);
  if (out.mode == climate::Mode::Alarm) {
    char al[160];
    snprintf(al, sizeof(al), "{\"zone\":\"%s\",\"temp\":%.2f,\"nh3\":%.2f,\"reason\":\"NH3/temperature out of range\"}",
             ZONE, meas.T, meas.nh3);
    mqtt.publish(topicAlarm, al);
  }
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  snprintf(topicTelemetry, sizeof(topicTelemetry), "%s/%s/telemetry", BASE, ZONE);
  snprintf(topicStatus, sizeof(topicStatus), "%s/%s/status", BASE, ZONE);
  snprintf(topicAlarm, sizeof(topicAlarm), "%s/alarm", BASE);
  snprintf(topicSetpoints, sizeof(topicSetpoints), "%s/setpoints", BASE);
  snprintf(topicCmd, sizeof(topicCmd), "%s/%s/cmd", BASE, ZONE);

  dhtIn.setup(PIN_DHT_IN, DHTesp::DHT22);
  dhtOut.setup(PIN_DHT_OUT, DHTesp::DHT22);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_ALARM_LED, OUTPUT);
  pwmInit(PIN_HEAT_LED, 0);
  pwmInit(PIN_FAN_LED, 1);

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setCallback(onMessage);
  connectMqtt();
  readSensors();
}

void loop() {
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();
  unsigned long now = millis();
  if (now - tSensor >= SENSOR_MS) { tSensor = now; readSensors(); }
  if (now - tControl >= CONTROL_MS) {
    tControl = now;
    out = ctrl.step(meas, CONTROL_MS / 1000.0);
    applyOutputs();
  }
  if (now - tPublish >= PUBLISH_MS) { tPublish = now; publishTelemetry(); }
}
