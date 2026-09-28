"""Відтворення результатів розділу 4.2 звіту (таблиця 5, рис. 9).

Запуск:
    python run_simulation.py                  # обидва алгоритми, таблиця 5
    python run_simulation.py --no-heat-limit  # запропонований алгоритм без обмеження V_тепл
    python run_simulation.py --plot fig9.png --csv results.csv
"""
import argparse, csv, math
import params as P
from poultry_house import PoultryHouse
from controllers import BaselineController, AdaptiveController, Measurements


def simulate(controller):
    house = PoultryHouse()
    n = int(P.DURATION // P.DT)
    log = {k: [] for k in ('t', 'T_out', 'T', 'nh3', 'co2', 'vent', 'q')}
    e_heat = e_fan = 0.0
    for k in range(n):
        t = k * P.DT
        t_out = P.t_outdoor(t)
        m = Measurements(t, house.T, t_out, house.nh3, house.co2)
        vent, q = controller.step(m, P.DT)
        house.step(t, t_out, vent, q, P.DT)
        e_heat += q * P.DT / 3.6e6
        e_fan += vent / P.FAN_STAGE * P.FAN_POWER_PER_STAGE * P.DT / 3600
        for key, v in (('t', t), ('T_out', t_out), ('T', house.T), ('nh3', house.nh3),
                       ('co2', house.co2), ('vent', vent), ('q', q)):
            log[key].append(v)
    T, nh3 = log['T'], log['nh3']
    heat_on = [q > 0 for q in log['q']]
    metrics = {
        'rmse_T': math.sqrt(sum((x - P.T_SETPOINT) ** 2 for x in T) / n),
        'T_min': min(T), 'T_max': max(T),
        'nh3_max': max(nh3), 'nh3_mean': sum(nh3) / n,
        'nh3_over20_pct': 100 * sum(x > 20 for x in nh3) / n,
        'co2_max': max(log['co2']),
        'heater_switches': sum(a != b for a, b in zip(heat_on, heat_on[1:])),
        'heat_kwh': e_heat, 'fan_kwh': e_fan,
    }
    return log, metrics


ROWS = [
    ('Середньоквадратичне відхилення T від уставки, °C', 'rmse_T', '{:.2f}'),
    ('Мінімальна температура, °C', 'T_min', '{:.1f}'),
    ('Максимальна температура, °C', 'T_max', '{:.1f}'),
    ('Максимальна концентрація NH3, ppm', 'nh3_max', '{:.1f}'),
    ('Середня концентрація NH3, ppm', 'nh3_mean', '{:.1f}'),
    ('Частка часу з NH3 > 20 ppm, %', 'nh3_over20_pct', '{:.1f}'),
    ('Максимальна концентрація CO2, ppm', 'co2_max', '{:.0f}'),
    ('Кількість циклів вмикання нагрівача', 'heater_switches', '{:d}'),
    ('Теплова енергія за добу, кВт*год', 'heat_kwh', '{:.0f}'),
    ('Електроенергія вентиляторів, кВт*год', 'fan_kwh', '{:.1f}'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--no-heat-limit', action='store_true', help='вимкнути обмеження V_тепл')
    ap.add_argument('--plot', help='зберегти графік (потрібен matplotlib)')
    ap.add_argument('--csv', help='зберегти часові ряди обох алгоритмів у CSV')
    a = ap.parse_args()

    log_a, m_a = simulate(BaselineController())
    log_b, m_b = simulate(AdaptiveController(heat_limit=not a.no_heat_limit))
    name_b = 'Запропонований' + (' (без V_тепл)' if a.no_heat_limit else '')
    print(f"{'Показник':52s} {'Базовий':>10s} {name_b:>16s}")
    for title, key, fmt in ROWS:
        print(f"{title:52s} {fmt.format(m_a[key]):>10s} {fmt.format(m_b[key]):>16s}")

    if a.csv:
        with open(a.csv, 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['t_h', 'T_out', 'A_T', 'A_nh3', 'A_co2', 'A_vent', 'A_q', 'B_T', 'B_nh3', 'B_co2', 'B_vent', 'B_q'])
            for i in range(len(log_a['t'])):
                w.writerow([f"{log_a['t'][i]/3600:.4f}", f"{log_a['T_out'][i]:.3f}"] +
                           [f"{log_a[k][i]:.4f}" for k in ('T', 'nh3', 'co2', 'vent', 'q')] +
                           [f"{log_b[k][i]:.4f}" for k in ('T', 'nh3', 'co2', 'vent', 'q')])
    if a.plot:
        import matplotlib; matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        th = [x / 3600 for x in log_a['t']]
        fig, ax = plt.subplots(2, 1, figsize=(8, 5.6), sharex=True)
        ax[0].plot(th, log_a['T'], color='#c05621', lw=1, label='Базовий: термостат ±1 °C + таймер вентиляції')
        ax[0].plot(th, log_b['T'], color='#2b6cb0', lw=1.6, label='Запропонований алгоритм')
        ax[0].axhline(P.T_SETPOINT, color='#555', ls='--', lw=0.8)
        ax[0].set_ylabel('Температура, °C'); ax[0].legend(fontsize=7, loc='lower right', frameon=False)
        ax[1].plot(th, log_a['nh3'], color='#c05621', lw=1)
        ax[1].plot(th, log_b['nh3'], color='#2b6cb0', lw=1.6)
        ax[1].axhline(20, color='#b91c1c', ls='--', lw=0.8)
        ax[1].set_ylabel('NH₃, ppm'); ax[1].set_xlabel('Час моделювання, год'); ax[1].set_xlim(0, 24)
        for x in ax: x.grid(alpha=0.25)
        fig.tight_layout(); fig.savefig(a.plot, dpi=200)


if __name__ == '__main__':
    main()
