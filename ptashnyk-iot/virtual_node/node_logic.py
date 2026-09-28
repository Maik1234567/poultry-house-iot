"""Логіка віртуального вузла ESP32 без мережевої частини (тестується окремо).

Вузол «вимірює» параметри з моделі пташника (model/), виконує запропонований
алгоритм керування і формує MQTT-повідомлення у форматі, описаному у звіті
(розд. 3.2): JSON з міткою часу, температурою, вологістю, NH3, CO2 і станом
виконавчих пристроїв.
"""
import json, os, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'model'))
import params as P                                    # noqa: E402
from poultry_house import PoultryHouse                # noqa: E402
from controllers import AdaptiveController, Measurements, heat_limited_ventilation  # noqa: E402

SETPOINT_RANGE = (15.0, 35.0)   # допустимі уставки температури, °C


class VirtualNode:
    def __init__(self, farm='farm', house='h1', zone='zoneA', speedup=60.0):
        self.base = f'{farm}/{house}'
        self.zone = zone
        self.speedup = speedup            # скільки модельних секунд на 1 реальну
        self.house = PoultryHouse()
        self.ctrl = AdaptiveController()
        self.t = 0.0                      # модельний час, с
        self.last = {'vent': 0.0, 'q': 0.0, 'mode': 'NORMAL'}

    # --- топіки -------------------------------------------------------------
    @property
    def topic_telemetry(self): return f'{self.base}/{self.zone}/telemetry'
    @property
    def topic_status(self): return f'{self.base}/{self.zone}/status'
    @property
    def topic_setpoints(self): return f'{self.base}/setpoints'
    @property
    def topic_cmd(self): return f'{self.base}/{self.zone}/cmd'
    @property
    def topic_alarm(self): return f'{self.base}/alarm'

    # --- керування ----------------------------------------------------------
    def advance(self, model_seconds):
        """Прокрутити модель і регулятор на model_seconds модельного часу."""
        steps = max(1, int(model_seconds // P.DT))
        for _ in range(steps):
            t_out = P.t_outdoor(self.t)
            m = Measurements(self.t, self.house.T, t_out, self.house.nh3, self.house.co2)
            vent, q = self.ctrl.step(m, P.DT)
            self.house.step(self.t, t_out, vent, q, P.DT)
            self.t += P.DT
        cap = max(heat_limited_ventilation(self.ctrl.sp, t_out), P.V_MIN)
        self.last = {'vent': vent, 'q': q, 'mode': self.mode(limited=abs(vent - cap) < 1e-9)}

    def mode(self, limited=False):
        """Режими з розд. 2.2 звіту (ті самі правила, що й у firmware/core)."""
        sp, T, nh3 = self.ctrl.sp, self.house.T, self.house.nh3
        if nh3 > P.NH3_LIMIT or abs(T - sp) > 4.0:
            return 'ALARM'
        if nh3 > 15.0 or T > sp + P.TUNNEL_DT:
            return 'BOOST_VENT'
        if limited:
            return 'HEAT_LIMITED'
        return 'NORMAL'

    def telemetry(self, now=None):
        """Повідомлення телеметрії (QoS 1)."""
        return json.dumps({
            'ts': int(now if now is not None else time.time()),
            'model_time_h': round(self.t / 3600, 3),
            'temp': round(self.house.T, 2),
            'hum': 62.0,               # вологість у моделі не розраховується: фіксоване значення
            'nh3': round(self.house.nh3, 2),
            'co2': round(self.house.co2),
            't_out': round(P.t_outdoor(self.t), 2),
            'setpoint': self.ctrl.sp,
            'vent_m3s': round(self.last['vent'], 3),
            'heater_w': round(self.last['q']),
            'mode': self.last['mode'],
        }, ensure_ascii=False)

    def alarm(self):
        """Повідомлення тривоги (QoS 2) або None."""
        if self.last['mode'] != 'ALARM':
            return None
        return json.dumps({'zone': self.zone, 'temp': round(self.house.T, 2),
                           'nh3': round(self.house.nh3, 2), 'reason': 'NH3/temperature out of range'})

    def handle_message(self, topic, payload: bytes):
        """Обробка вхідних повідомлень. Повертає рядок-результат для журналу."""
        try:
            data = json.loads(payload.decode('utf-8'))
        except (ValueError, UnicodeDecodeError):
            return 'ignored: not JSON'
        if topic == self.topic_setpoints or topic == self.topic_cmd:
            sp = data.get('temp_setpoint')
            if sp is None:
                return 'ignored: no temp_setpoint'
            try:
                sp = float(sp)
            except (TypeError, ValueError):
                return 'ignored: bad value'
            if not SETPOINT_RANGE[0] <= sp <= SETPOINT_RANGE[1]:
                return f'rejected: {sp} outside {SETPOINT_RANGE}'
            self.ctrl.sp = sp
            return f'setpoint={sp}'
        return 'ignored: unknown topic'
