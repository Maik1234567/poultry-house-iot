"""Одновузлова (повне перемішування) модель теплового та газового балансу пташника."""
import params as P


class PoultryHouse:
    def __init__(self):
        self.T = P.T_INIT
        self.nh3 = P.NH3_INIT
        self.co2 = P.CO2_INIT

    def step(self, t, t_out, vent, q_heat, dt):
        """Явний метод Ейлера.
        C*dT/dt   = Q_нагр + Q_пт - (UA + rho*c_p*V)*(T - T_зовн)
        Vпр*dN/dt = G_NH3(t) - V*N
        Vпр*dC/dt = G_CO2 - V*(C - C_зовн)
        """
        dT = (q_heat + P.Q_BIRDS - (P.UA + P.RHO_CP * vent) * (self.T - t_out)) / P.HEAT_CAPACITY
        self.T += dT * dt
        g_nh3 = P.NH3_GEN_BASE * (1 + P.NH3_GEN_GROWTH * t / 86400.0)
        self.nh3 += (g_nh3 - vent * self.nh3) / P.AIR_VOLUME * dt
        self.co2 += (P.CO2_GEN - vent * (self.co2 - P.CO2_OUTDOOR)) / P.AIR_VOLUME * dt
