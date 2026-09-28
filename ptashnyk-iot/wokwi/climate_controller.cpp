#include "climate_controller.h"

namespace climate {

static inline real_t clip(real_t x, real_t lo, real_t hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline real_t maxr(real_t a, real_t b) { return a > b ? a : b; }
static inline real_t minr(real_t a, real_t b) { return a < b ? a : b; }

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Normal: return "NORMAL";
        case Mode::BoostVentilation: return "BOOST_VENT";
        case Mode::HeatLimited: return "HEAT_LIMITED";
        case Mode::Alarm: return "ALARM";
    }
    return "?";
}

real_t heatLimitedVentilation(const Params& p, real_t t_set, real_t t_out) {
    real_t dtemp = maxr(t_set - p.dt_allowed - t_out, 1.0);  // захист від ділення на 0
    return ((p.q_heater_max + p.q_birds) / dtemp - p.ua) / p.rho_cp;
}

AdaptiveController::AdaptiveController(const Params& p, real_t setpoint)
    : p_(p), sp_(setpoint), i_heat_(p.heat_i_init) {}

Output AdaptiveController::step(const Measurements& m, real_t dt) {
    Output out;
    bool limited = false;
    // 1) ПІ-регулятор аміаку
    real_t e = m.nh3 - p_.nh3_setpoint;
    i_vent_ = clip(i_vent_ + p_.nh3_ki * e * dt, 0.0, p_.nh3_i_max);
    real_t vent = clip(p_.v_min + p_.nh3_kp * e + i_vent_, p_.v_min, p_.v_max);
    // 2) складова CO2
    if (m.co2 > p_.co2_threshold)
        vent = maxr(vent, p_.v_min + (m.co2 - p_.co2_threshold) * p_.co2_gain);
    // 3) обмеження за тепловим балансом
    if (p_.heat_limit && m.nh3 < p_.nh3_limit) {
        real_t cap = maxr(heatLimitedVentilation(p_, sp_, m.T_out), p_.v_min);
        if (cap < vent) { vent = cap; limited = true; }
    }
    // 4) перегрів
    if (m.T > sp_ + p_.tunnel_dt) vent = maxr(vent, p_.v_max);
    // 5) ПІ-регулятор нагріву
    real_t eh = sp_ - m.T;
    i_heat_ = clip(i_heat_ + p_.heat_ki * eh * dt, 0.0, p_.q_heater_max);
    out.q_heat = clip(p_.heat_kp * eh + i_heat_, 0.0, p_.q_heater_max);
    out.vent = vent;
    // режим (для індикації та сповіщень)
    real_t dev = m.T - sp_; if (dev < 0) dev = -dev;
    if (m.nh3 > p_.nh3_limit || dev > p_.alarm_dt) out.mode = Mode::Alarm;
    else if (m.nh3 > p_.boost_nh3 || m.T > sp_ + p_.tunnel_dt) out.mode = Mode::BoostVentilation;
    else if (limited) out.mode = Mode::HeatLimited;
    else out.mode = Mode::Normal;
    return out;
}

Output BaselineController::step(const Measurements& m, real_t t) {
    if (m.T < sp_ - 1.0) heat_on_ = true;
    if (m.T > sp_ + 1.0) heat_on_ = false;
    Output out;
    out.q_heat = heat_on_ ? q_max_ : 0.0;
    // t % 600 < 300 без <cmath> (t >= 0)
    real_t phase = t - 600.0 * static_cast<long>(t / 600.0);
    out.vent = phase < 300.0 ? 2.8 : 0.0;
    if (m.T > sp_ + 3.0) out.vent = 5.6;
    out.mode = Mode::Normal;
    return out;
}

}  // namespace climate
