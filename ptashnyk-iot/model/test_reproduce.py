"""Перевірка: модель відтворює числа з таблиці 5 звіту.  Запуск: python test_reproduce.py"""
from run_simulation import simulate
from controllers import BaselineController, AdaptiveController

EXPECTED = {  # значення з таблиці 5 звіту (з округленням, як у звіті)
    'A': {'rmse_T': (0.63, 2), 'T_min': (21.6, 1), 'T_max': (24.0, 1), 'nh3_max': (25.3, 1),
          'nh3_mean': (17.1, 1), 'nh3_over20_pct': (22.7, 1), 'heater_switches': (82, 0), 'heat_kwh': (1190, 0)},
    'B': {'rmse_T': (0.41, 2), 'T_min': (22.0, 1), 'T_max': (23.1, 1), 'nh3_max': (14.8, 1),
          'nh3_mean': (11.5, 1), 'nh3_over20_pct': (0.0, 1), 'heater_switches': (0, 0), 'heat_kwh': (1713, 0)},
    'B_no_limit': {'T_min': (16.8, 1)},
}

def check(name, metrics):
    ok = True
    for key, (val, nd) in EXPECTED[name].items():
        got = round(metrics[key], nd)
        good = abs(got - val) < 1e-9
        ok &= good
        print(f"{'OK ' if good else 'ERR'} {name:11s} {key:16s} звіт={val}  модель={got}")
    return ok

if __name__ == '__main__':
    ok = check('A', simulate(BaselineController())[1])
    ok &= check('B', simulate(AdaptiveController())[1])
    ok &= check('B_no_limit', simulate(AdaptiveController(heat_limit=False))[1])
    print('\nУСІ ПЕРЕВІРКИ ПРОЙДЕНО' if ok else '\nЄ РОЗБІЖНОСТІ')
    raise SystemExit(0 if ok else 1)
