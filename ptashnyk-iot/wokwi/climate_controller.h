// Ядро алгоритму керування мікрокліматом пташника.
// Не залежить від Arduino/ESP-IDF: той самий код компілюється для ESP32
// і для ПК (тест із моделлю приміщення, див. test/closed_loop_test.cpp).
#pragma once

namespace climate {

typedef double real_t;  // на ESP32 можна замінити на float

struct Params {
    // приміщення / обладнання
    real_t q_heater_max = 80000.0;  // Вт
    real_t q_birds      = 20000.0;  // оцінка явного тепловиділення птиці, Вт
    real_t ua           = 800.0;    // Вт/К
    real_t rho_cp       = 1.2 * 1005.0; // Дж/(м3*К)
    // вентиляція
    real_t v_min = 0.9, v_max = 5.6;    // м3/с
    // ПІ-регулятор аміаку
    real_t nh3_setpoint = 10.0, nh3_kp = 0.12, nh3_ki = 2e-5, nh3_i_max = 2.0;
    // складова CO2
    real_t co2_threshold = 2500.0, co2_gain = 1.0 / 500.0;
    // обмеження за тепловим балансом
    real_t nh3_limit = 20.0, dt_allowed = 1.0;
    bool   heat_limit = true;
    // перегрів
    real_t tunnel_dt = 2.0;
    // ПІ-регулятор нагріву
    real_t heat_kp = 8000.0, heat_ki = 6.0, heat_i_init = 45000.0;
    // пороги режимів (розд. 2.2 звіту)
    real_t boost_nh3 = 15.0, alarm_dt = 4.0;
};

struct Measurements {
    real_t T;      // °C, у приміщенні
    real_t T_out;  // °C, зовні
    real_t nh3;    // ppm
    real_t co2;    // ppm
};

enum class Mode { Normal, BoostVentilation, HeatLimited, Alarm };

struct Output {
    real_t vent;     // потрібна продуктивність вентиляції, м3/с
    real_t q_heat;   // потрібна потужність нагріву, Вт
    Mode   mode;
};

const char* modeName(Mode m);

// Гранична вентиляція, яку компенсують нагрівачі:
// V_тепл = ((Q_max + Q_пт)/(T_уст - dT - T_зовн) - UA) / (rho*c_p)
real_t heatLimitedVentilation(const Params& p, real_t t_set, real_t t_out);

class AdaptiveController {
public:
    explicit AdaptiveController(const Params& p = Params(), real_t setpoint = 23.0);
    Output step(const Measurements& m, real_t dt);
    void   setSetpoint(real_t sp) { sp_ = sp; }
    real_t setpoint() const { return sp_; }
    const Params& params() const { return p_; }
private:
    Params p_;
    real_t sp_;
    real_t i_vent_ = 0.0;
    real_t i_heat_;
};

// Базовий алгоритм для порівняння: термостат ±1 °C + таймер 5 хв з 10.
class BaselineController {
public:
    BaselineController(real_t setpoint = 23.0, real_t q_max = 80000.0) : sp_(setpoint), q_max_(q_max) {}
    Output step(const Measurements& m, real_t t_seconds);
private:
    real_t sp_, q_max_;
    bool heat_on_ = false;
};

}  // namespace climate
