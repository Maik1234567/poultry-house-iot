// Замкнений контур на ПК: ядро алгоритму (як на ESP32) + модель пташника.
// Виводить показники таблиці 5 і пише CSV для порівняння з Python-моделлю.
#include "../core/climate_controller.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using namespace climate;

struct House {  // та сама модель, що model/poultry_house.py
    double T = 23.0, nh3 = 10.0, co2 = 2000.0;
    void step(double t, double t_out, double vent, double q, double dt) {
        const double C = 20e6, UA = 800.0, RC = 1.2 * 1005.0, QB = 20000.0, V = 1800.0;
        T += (q + QB - (UA + RC * vent) * (T - t_out)) / C * dt;
        double g = 11.7 / 0.7 * (1 + 0.9 * t / 86400.0);
        nh3 += (g - vent * nh3) / V * dt;
        co2 += (1.39 * 1600.0 - vent * (co2 - 420.0)) / V * dt;
    }
};

static double tout(double t) { return -5.0 + 4.0 * std::sin(2 * M_PI * (t / 3600.0 - 9.0) / 24.0); }

template <class Ctrl, class StepFn>
static void run(const char* name, Ctrl& c, StepFn stepfn, FILE* csv) {
    House h; const double dt = 10.0; const int n = 24 * 3600 / 10;
    double se = 0, tmin = 1e9, tmax = -1e9, nmax = 0, nsum = 0, eh = 0; int over = 0, sw = 0; bool prev = false;
    int modes[4] = {0, 0, 0, 0};
    for (int k = 0; k < n; ++k) {
        double t = k * dt, to = tout(t);
        Measurements m{h.T, to, h.nh3, h.co2};
        Output o = stepfn(c, m, t, dt);
        h.step(t, to, o.vent, o.q_heat, dt);
        eh += o.q_heat * dt / 3.6e6;
        bool on = o.q_heat > 0; if (k > 0 && on != prev) ++sw; prev = on;
        se += (h.T - 23.0) * (h.T - 23.0); tmin = std::fmin(tmin, h.T); tmax = std::fmax(tmax, h.T);
        nmax = std::fmax(nmax, h.nh3); nsum += h.nh3; if (h.nh3 > 20) ++over;
        modes[static_cast<int>(o.mode)]++;
        if (csv) std::fprintf(csv, "%s,%.1f,%.10f,%.10f,%.10f,%.10f,%s\n", name, t, h.T, h.nh3, o.vent, o.q_heat, modeName(o.mode));
    }
    std::printf("%-22s RMSE=%.2f  T=[%.1f; %.1f]  NH3max=%.1f  NH3mean=%.1f  >20ppm=%.1f%%  вмикань=%d  тепло=%.0f кВт*год\n",
                name, std::sqrt(se / n), tmin, tmax, nmax, nsum / n, 100.0 * over / n, sw, eh);
    if (std::strcmp(name, "adaptive") == 0)
        std::printf("   час у режимах, %%: NORMAL=%.1f BOOST=%.1f HEAT_LIMITED=%.1f ALARM=%.1f\n",
                    100.0 * modes[0] / n, 100.0 * modes[1] / n, 100.0 * modes[2] / n, 100.0 * modes[3] / n);
}

int main(int argc, char** argv) {
    FILE* csv = argc > 1 ? std::fopen(argv[1], "w") : nullptr;
    if (csv) std::fprintf(csv, "algo,t,T,nh3,vent,q,mode\n");
    BaselineController base;
    run("baseline", base, [](BaselineController& c, const Measurements& m, double t, double) { return c.step(m, t); }, csv);
    AdaptiveController ad;
    run("adaptive", ad, [](AdaptiveController& c, const Measurements& m, double, double dt) { return c.step(m, dt); }, csv);
    Params p; p.heat_limit = false; AdaptiveController nolim(p);
    run("adaptive_no_limit", nolim, [](AdaptiveController& c, const Measurements& m, double, double dt) { return c.step(m, dt); }, nullptr);
    if (csv) std::fclose(csv);
    return 0;
}
