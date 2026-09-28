// Перевірка JS-коду функціональних вузлів Node-RED без Node-RED (імітація msg/env/context).
const flows = require('./node-red/flows.json');
const assert = require('assert');
const fn = id => flows.find(n => n.id === id).func;
function run(id, msg, envv = {}) {
  const store = {};
  const context = { get: k => store[k], set: (k, v) => { store[k] = v; } };
  const env = { get: k => envv[k] };
  return new Function('msg', 'env', 'context', 'node', fn(id))(msg, env, context, { warn() {} });
}
const tel = { ts: 1727500000, temp: 22.9, hum: 62, nh3: 10.9, co2: 2150, t_out: -3.1, setpoint: 23, vent_m3s: 1.2, heater_w: 52000, mode: 'NORMAL' };

// 1) у нормі тривоги немає
assert.strictEqual(run('fn_check', { topic: 'farm/h1/zoneA/telemetry', payload: tel }), null);
// 2) NH3 > 20 -> тривога в farm/h1/alarm, QoS 2
const a = run('fn_check', { topic: 'farm/h1/zoneA/telemetry', payload: { ...tel, nh3: 25.3 } });
assert.strictEqual(a.topic, 'farm/h1/alarm'); assert.strictEqual(a.qos, 2);
assert.ok(a.payload.problems[0].startsWith('NH3 25.3'));
// 3) InfluxDB line protocol
const w = run('fn_influx', { topic: 'farm/h1/zoneA/telemetry', payload: tel }, { INFLUX_URL: 'http://influxdb:8086', INFLUX_TOKEN: 't' });
assert.strictEqual(w.payload, 'climate,house=h1,zone=zoneA,mode=NORMAL temp=22.9,hum=62,nh3=10.9,co2=2150,t_out=-3.1,setpoint=23,vent_m3s=1.2,heater_w=52000 1727500000');
assert.strictEqual(w.headers.Authorization, 'Token t');
console.log('OK: функції Node-RED (перевірка меж, InfluxDB) працюють коректно');
