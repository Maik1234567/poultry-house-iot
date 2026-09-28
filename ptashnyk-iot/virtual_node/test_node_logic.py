"""Тести логіки вузла без брокера.  Запуск: python test_node_logic.py"""
import json
from node_logic import VirtualNode

n = VirtualNode()
assert n.topic_telemetry == 'farm/h1/zoneA/telemetry'
assert n.topic_setpoints == 'farm/h1/setpoints'

# 1) телеметрія — коректний JSON з потрібними полями
n.advance(1800)
d = json.loads(n.telemetry(now=0))
for k in ('ts', 'temp', 'hum', 'nh3', 'co2', 'setpoint', 'vent_m3s', 'heater_w', 'mode'):
    assert k in d, k
assert d['model_time_h'] == 0.5 and 20 < d['temp'] < 25 and 5 < d['nh3'] < 20

# 2) уставки: прийняти коректну, відхилити поза межами і сміття
assert n.handle_message('farm/h1/setpoints', b'{"temp_setpoint": 24.5}') == 'setpoint=24.5'
assert n.ctrl.sp == 24.5
assert n.handle_message('farm/h1/setpoints', b'{"temp_setpoint": 60}').startswith('rejected')
assert n.handle_message('farm/h1/zoneA/cmd', b'not json') == 'ignored: not JSON'
assert n.ctrl.sp == 24.5

# 3) після зміни уставки регулятор підтягує температуру
n.advance(4 * 3600)
assert abs(n.house.T - 24.5) < 0.6, n.house.T

# 4) доба роботи = ті самі результати, що й модель: NH3 < 20 ppm, тривог немає
n2 = VirtualNode()
alarms = 0
for _ in range(48):
    n2.advance(1800)
    alarms += n2.alarm() is not None
assert alarms == 0 and n2.house.nh3 < 20
print('OK: усі тести логіки вузла пройдено; кінець доби:', n2.telemetry(now=0))
