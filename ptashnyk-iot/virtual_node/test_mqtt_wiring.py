"""Перевірка MQTT-обв'язки virtual_node.py на імітації бібліотеки paho (без брокера)."""
import sys, types, json

calls = []
class FakeClient:
    def __init__(self, *a, **k): self.on_connect = self.on_message = None
    def will_set(self, *a, **k): calls.append(('will', a, k))
    def connect(self, *a, **k): calls.append(('connect', a))
    def publish(self, *a, **k): calls.append(('pub', a, k))
    def subscribe(self, *a, **k): calls.append(('sub', a))
mod = types.ModuleType('paho.mqtt.client')
mod.Client = FakeClient
mod.CallbackAPIVersion = types.SimpleNamespace(VERSION2=2)
sys.modules.update({'paho': types.ModuleType('paho'), 'paho.mqtt': types.ModuleType('paho.mqtt'), 'paho.mqtt.client': mod})

from node_logic import VirtualNode
from virtual_node import make_client
node = VirtualNode()
c = make_client(node, 'localhost', 1883)
c.on_connect(c, None, {}, 0, None)                      # сигнатура paho 2.x
c.on_message(c, None, types.SimpleNamespace(topic='farm/h1/setpoints', payload=b'{"temp_setpoint": 22}'))
assert calls[0] == ('will', ('farm/h1/zoneA/status', 'offline'), {'qos': 1, 'retain': True})
assert ('pub', ('farm/h1/zoneA/status', 'online'), {'qos': 1, 'retain': True}) in calls
assert ('sub', ([('farm/h1/setpoints', 1), ('farm/h1/zoneA/cmd', 1)],)) in calls
assert node.ctrl.sp == 22.0
print('OK: LWT, підписки, публікація статусу й обробка уставки працюють')
