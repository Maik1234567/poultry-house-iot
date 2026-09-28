"""Віртуальний вузол ESP32: публікує телеметрію в MQTT і приймає уставки.

Встановлення:  pip install paho-mqtt
Запуск:        python virtual_node.py --host localhost --interval 30 --speedup 60
(кожні 30 с реального часу модель просувається на 30*60 с = 30 хв)
"""
import argparse, time
from node_logic import VirtualNode


def make_client(node, host, port):
    import paho.mqtt.client as mqtt
    try:  # paho-mqtt >= 2.0
        c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f'virtual-{node.zone}')
    except AttributeError:  # paho-mqtt 1.x
        c = mqtt.Client(client_id=f'virtual-{node.zone}')
    # «остання воля»: брокер сам опублікує offline, якщо вузол зникне
    c.will_set(node.topic_status, 'offline', qos=1, retain=True)

    def on_connect(client, userdata, flags, rc, *args):
        client.publish(node.topic_status, 'online', qos=1, retain=True)
        client.subscribe([(node.topic_setpoints, 1), (node.topic_cmd, 1)])
        print('connected, rc =', rc)

    def on_message(client, userdata, msg):
        print(msg.topic, '->', node.handle_message(msg.topic, msg.payload))

    c.on_connect = on_connect
    c.on_message = on_message
    c.connect(host, port, keepalive=60)
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='localhost')
    ap.add_argument('--port', type=int, default=1883)
    ap.add_argument('--interval', type=float, default=30.0, help='період публікації, с')
    ap.add_argument('--speedup', type=float, default=60.0, help='прискорення модельного часу')
    a = ap.parse_args()
    node = VirtualNode(speedup=a.speedup)
    client = make_client(node, a.host, a.port)
    client.loop_start()
    try:
        while True:
            node.advance(a.interval * a.speedup)
            client.publish(node.topic_telemetry, node.telemetry(), qos=1)
            alarm = node.alarm()
            if alarm:
                client.publish(node.topic_alarm, alarm, qos=2)
            print(node.telemetry())
            time.sleep(a.interval)
    except KeyboardInterrupt:
        client.publish(node.topic_status, 'offline', qos=1, retain=True)
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == '__main__':
    main()
