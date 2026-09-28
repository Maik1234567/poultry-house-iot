"""Алгоритми керування: базовий (термостат + таймер) і запропонований."""
from dataclasses import dataclass
import params as P


def clip(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


@dataclass
class Measurements:
    t: float        # час від початку, с
    T: float        # температура в приміщенні, °C
    T_out: float    # зовнішня температура, °C
    nh3: float      # ppm
    co2: float      # ppm


class BaselineController:
    """Двопозиційний термостат з гістерезисом ±1 °C і таймерна вентиляція 5 хв з 10."""

    def __init__(self, setpoint=P.T_SETPOINT):
        self.sp = setpoint
        self.heat_on = False

    def step(self, m: Measurements, dt: float):
        if m.T < self.sp - P.BASE_HYST:
            self.heat_on = True
        if m.T > self.sp + P.BASE_HYST:
            self.heat_on = False
        q = P.Q_HEATER_MAX if self.heat_on else 0.0
        vent = P.FAN_STAGE if (m.t % P.BASE_TIMER_PERIOD) < P.BASE_TIMER_ON else 0.0
        if m.T > self.sp + P.BASE_COOL_DT:
            vent = 2 * P.FAN_STAGE
        return vent, q


def heat_limited_ventilation(t_set, t_out):
    """Гранична вентиляція, яку ще здатні компенсувати нагрівачі (див. звіт, розд. 3.3).

    З рівняння теплового балансу в усталеному режимі
        Q_max + Q_пт = (UA + rho*c_p*V) * (T_уст - dT - T_зовн)
    =>  V_тепл = ((Q_max + Q_пт) / (T_уст - dT - T_зовн) - UA) / (rho*c_p)
    Знаменник обмежено знизу 1 °C, щоб уникнути ділення на нуль у теплу погоду.
    """
    dtemp = max(t_set - P.DT_ALLOWED - t_out, 1.0)
    return ((P.Q_HEATER_MAX + P.Q_BIRDS) / dtemp - P.UA) / P.RHO_CP


class AdaptiveController:
    """Запропонований алгоритм: V = max(V_min, V_NH3, V_CO2), обмеження V_тепл, ПІ-нагрів."""

    def __init__(self, setpoint=P.T_SETPOINT, heat_limit=True):
        self.sp = setpoint
        self.heat_limit = heat_limit
        self.i_vent = 0.0              # інтегратор ПІ-регулятора аміаку, м3/с
        self.i_heat = P.HEAT_I_INIT    # інтегратор ПІ-регулятора нагріву, Вт

    def step(self, m: Measurements, dt: float):
        # 1) ПІ-регулятор аміаку (уставка 10 ppm), обмеження інтегратора 0..2 м3/с
        e = m.nh3 - P.NH3_SETPOINT
        self.i_vent = clip(self.i_vent + P.NH3_KI * e * dt, 0.0, P.NH3_I_MAX)
        vent = clip(P.V_MIN + P.NH3_KP * e + self.i_vent, P.V_MIN, P.V_MAX)
        # 2) складова CO2: V_CO2 = V_min + (CO2 - 2500)/500 при CO2 > 2500 ppm
        if m.co2 > P.CO2_THRESHOLD:
            vent = max(vent, P.V_MIN + (m.co2 - P.CO2_THRESHOLD) * P.CO2_GAIN)
        # 3) обмеження за тепловим балансом, поки NH3 < 20 ppm
        if self.heat_limit and m.nh3 < P.NH3_LIMIT:
            vent = min(vent, max(heat_limited_ventilation(self.sp, m.T_out), P.V_MIN))
        # 4) перегрів -> максимальна вентиляція
        if m.T > self.sp + P.TUNNEL_DT:
            vent = max(vent, P.V_MAX)
        # 5) ПІ-регулятор нагріву, вихід 0..Q_max, anti-windup обмеженням інтегратора
        e = self.sp - m.T
        self.i_heat = clip(self.i_heat + P.HEAT_KI * e * dt, 0.0, P.Q_HEATER_MAX)
        q = clip(P.HEAT_KP * e + self.i_heat, 0.0, P.Q_HEATER_MAX)
        return vent, q
