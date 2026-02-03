# Linux Backend (NetMon Collector)

This directory contains the Linux-side runtime configuration for the ESP32 NetMon project.

## Overview
The Linux backend subscribes to MQTT metrics published by ESP32 devices and persists them
to disk for dashboards and analysis.

Components:
- Mosquitto MQTT broker
- Python collector running as a systemd service
- Log rotation via logrotate

---

## Collector Service

**Service name:** `netmon-collector.service`  
**Install path:** `/etc/systemd/system/netmon-collector.service`  
**Runtime user:** `netmon`  
**Working directory:** `/opt/netmon`  
**Python:** `/opt/netmon/venv/bin/python`

### Environment variables
- `MQTT_HOST=127.0.0.1`
- `MQTT_PORT=1883`
- `MQTT_TOPIC=netmon/+/metrics`
- `NETMON_OUT_DIR=/var/log/netmon`

### Service management
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now netmon-collector
sudo systemctl status netmon-collector
