# IoT-система контролю мікроклімату пташника — програмна частина

Код до звіту з проєктно-технологічної практики (Маїк Михайло, ІР-45, НУ «Львівська політехніка»).

| Каталог | Що містить | Як перевірено |
|---|---|---|
| `model/` | Імітаційна модель пташника та обидва алгоритми керування (Python, лише стандартна бібліотека). Відтворює таблицю 5 і рис. 9 звіту | `python test_reproduce.py` |
| `firmware/core/` | Ядро алгоритму керування на C++11 без залежностей від Arduino — призначене для перенесення на ESP32 | `make test` (замкнений контур з моделлю на ПК) |
| `virtual_node/` | Віртуальний вузол ESP32: запускає модель і алгоритм, публікує телеметрію в MQTT, приймає уставки, має LWT | `python test_node_logic.py`, `python test_mqtt_wiring.py` |
| `wokwi/` | Скетч ESP32 для симулятора Wokwi: DHT22, потенціометри, світлодіоди, реле, MQTT (див. `wokwi/README.md`) | `make -C wokwi/test_host test` (компіляція на ПК із заглушками) |
| `server/` | Mosquitto, Node-RED (перевірка меж, запис в InfluxDB, тривоги, вебпанель), InfluxDB — `docker compose up` | `node test_flow_functions.js` |

## 1. Відтворення результатів моделювання (розд. 4.2)

```bash
cd model
python run_simulation.py                  # таблиця 5 звіту
python run_simulation.py --no-heat-limit  # варіант без обмеження V_тепл (мінімум 16,8 °C)
python test_reproduce.py                  # автоматична звірка з числами звіту
python run_simulation.py --plot fig9.png --csv results.csv   # графік (потрібен matplotlib) і часові ряди
```

Усі параметри моделі та регуляторів — у файлі `model/params.py`. Модель детермінована: результат однаковий за кожного запуску.

## 2. Ядро алгоритму на C++

```bash
cd firmware
make test
```
Програма `test/closed_loop_test.cpp` з'єднує ядро алгоритму з тією самою моделлю пташника, переписаною на C++, і виводить показники таблиці 5. Часові ряди збігаються з Python-моделлю з точністю ~1e-10.

## 3. Повний ланцюжок: вузол → MQTT → Node-RED → InfluxDB → вебпанель

Потрібен Docker Desktop.
```bash
cd server
docker compose up
```
* Вебпанель: http://localhost:1880/panel (показує телеметрію, дозволяє змінити уставку)
* Редактор Node-RED: http://localhost:1880
* InfluxDB: http://localhost:8086 (admin / admin12345), bucket `climate`

Віртуальний вузол публікує телеметрію кожні 5 с; модельний час прискорено в 360 разів (1 доба ≈ 4 хв).
Без Docker вузол можна запустити окремо: `pip install paho-mqtt`, потім `python virtual_node/virtual_node.py --host <адреса брокера>`.

### MQTT-топіки
| Топік | Напрям | QoS | Вміст |
|---|---|---|---|
| `farm/h1/zoneA/telemetry` | вузол → | 1 | JSON: ts, temp, hum, nh3, co2, t_out, setpoint, vent_m3s, heater_w, mode |
| `farm/h1/zoneA/status` | вузол → (retain, LWT) | 1 | `online` / `offline` |
| `farm/h1/alarm` | вузол, Node-RED → | 2 | JSON з причиною тривоги |
| `farm/h1/setpoints` | → вузол (retain) | 1 | `{"temp_setpoint": 23.0}` |

## Обмеження (чесно)
* Фізичний ESP32 з давачами SHT31/SCD30/MQ-137 не використовувався. Замкнений контур перевірено з віртуальним вузлом; роботу ядра на ESP32 можна перевірити в симуляторі Wokwi (папка `wokwi/`), де давачі замінено на DHT22 і потенціометри.
* Параметри моделі — розрахункові оцінки, а не заміри з реального пташника. Вологість у моделі не розраховується (у телеметрії — фіксоване значення).
* Конфігурація Mosquitto навчальна (без паролів і TLS).
