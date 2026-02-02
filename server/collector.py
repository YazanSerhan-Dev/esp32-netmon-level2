#!/usr/bin/env python3
import json
import os
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

BROKER_HOST = os.getenv("MQTT_HOST", "127.0.0.1")
BROKER_PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC = os.getenv("MQTT_TOPIC", "netmon/+/metrics")

OUT_DIR = os.getenv("NETMON_OUT_DIR", "/var/log/netmon")
LOG_FILE = os.path.join(OUT_DIR, "metrics.log")
LATEST_FILE = os.path.join(OUT_DIR, "latest.json")

def utc_ts() -> str:
    # ISO8601 UTC timestamp
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

def ensure_out_dir():
    os.makedirs(OUT_DIR, exist_ok=True)

def append_line(line: str):
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(line + "\n")

def write_latest(obj: dict):
    tmp = LATEST_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False)
        f.write("\n")
    os.replace(tmp, LATEST_FILE)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[{utc_ts()}] Connected to MQTT {BROKER_HOST}:{BROKER_PORT}, subscribing to {TOPIC}")
        client.subscribe(TOPIC)
    else:
        print(f"[{utc_ts()}] MQTT connect failed rc={rc}")

def on_message(client, userdata, msg):
    now = utc_ts()
    topic = msg.topic
    payload_raw = msg.payload.decode("utf-8", errors="replace")

    # Try parse JSON
    try:
        data = json.loads(payload_raw)
    except json.JSONDecodeError:
        # Still log raw
        line = f"{now} topic={topic} raw={payload_raw}"
        append_line(line)
        return

    # Enrich / normalize
    data_out = {
        "ts": now,
        "topic": topic,
        "device": topic.split("/")[1] if topic.count("/") >= 2 else "unknown",
        "rssi": data.get("rssi"),
        "router_ms": data.get("router_ms"),
        "linux_ms": data.get("linux_ms"),
        "state": data.get("state"),
    }

    # Write human log line
    line = (
        f"{now} device={data_out['device']} "
        f"rssi={data_out['rssi']} router_ms={data_out['router_ms']} "
        f"linux_ms={data_out['linux_ms']} state={data_out['state']}"
    )
    append_line(line)

    # Update latest.json
    write_latest(data_out)

def main():
    ensure_out_dir()

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    # Auto-reconnect loop
    while True:
        try:
            client.connect(BROKER_HOST, BROKER_PORT, keepalive=30)
            client.loop_forever()
        except Exception as e:
            print(f"[{utc_ts()}] Collector error: {e}")
            time.sleep(2)

if __name__ == "__main__":
    main()
