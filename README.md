# ESP32 NetMon – Embedded Network Monitoring System

## Overview
ESP32 NetMon is a **real-world embedded network monitoring system** built with an ESP32 device and a Linux backend.  
The project measures network health (WiFi quality and reachability), visualizes system states locally on hardware (LEDs + LCD), and collects metrics on a Linux server with logging, rotation, and a web-based graph dashboard.

This project is designed to demonstrate **embedded systems**, **networking**, **Linux services**, and **observability concepts** in a production-like setup.

---

## High-Level Architecture

```
ESP32
  │  WiFi
  ▼
MQTT Broker (Mosquitto)
  │
  ▼
Linux Collector (Python + systemd)
  │
  ├── /var/log/netmon/metrics.log
  └── /var/log/netmon/latest.json
  │
  ▼
Web Dashboard (Graphs)
```

---

## Features

### ESP32 (Embedded)
- Connects to WiFi and monitors connection quality
- Measures:
  - RSSI (WiFi signal strength)
  - Latency to router (ping)
  - Latency to Linux server (ping)
- Displays status using:
  - **LEDs (GPIO 16–19)**
  - **I2C LCD (16x2)**
- Publishes metrics periodically using **MQTT**
- Fully **non-blocking design** (no freezes when Linux or MQTT is down)

### System States
| State | Meaning | LED |
|-----|-------|----|
| OK | WiFi + router + Linux reachable | Green |
| DEGRADED | WiFi + router OK, Linux unreachable or high latency | Yellow |
| DOWN | WiFi disconnected or router unreachable | Red |
| RECOVERING | Temporary recovery window / RTO display | Blue (blinking) |

Linux being down **never causes DOWN by itself**.

---

## MQTT Layer
- Broker: **Mosquitto** (Linux)
- Topic format:
  ```
  netmon/<device-id>/metrics
  ```
- JSON payload example:
  ```json
  {
    "device": "esp32-1",
    "ts": "2026-02-03T12:00:01Z",
    "rssi": -52,
    "router_ms": 3,
    "linux_ms": 8,
    "state": "OK"
  }
  ```

---

## Linux Backend

### Collector Service
- Implemented in Python (`collector.py`)
- Runs as a **systemd service**
- Subscribes to all ESP32 devices via MQTT
- Writes:
  - `/var/log/netmon/metrics.log` (historical data)
  - `/var/log/netmon/latest.json` (latest snapshot)

### Logging & Reliability
- Logs stored under `/var/log/netmon/`
- **logrotate** configured to:
  - Prevent unlimited log growth
  - Rotate and compress old logs
- systemd ensures:
  - Auto-start on boot
  - Automatic restart on failure

---

## Web Dashboard (Graphs)
- Served from the Linux backend
- Reads data via REST API:
  - `/api/latest`
  - `/api/history?n=...`
- Displays:
  - Live graphs for RSSI, router latency, Linux latency
  - Current device state
- Auto-refreshing every few seconds

---

## Repository Structure

```
.
├── esp32-netmon/        # ESP32 firmware (PlatformIO)
├── server/              # Linux backend (collector + API)
├── dashboard/           # Web dashboard (graphs)
├── linux/               # systemd & deployment files
├── docs/                # Architecture documentation
└── README.md
```

---

## Engineering Principles
- Non-blocking embedded design
- Time-based state detection (not failure counters)
- Clear separation between runtime data and source code
- Linux-standard filesystem layout
- Production-style logging and service management

---

## Security Notes
- WiFi credentials are stored in a local `secrets.h` file on the ESP32
- Secrets are **not committed** to GitHub
- Logs and runtime data are not version-controlled

---

## Use Cases
- Embedded systems learning
- Network health monitoring
- IoT + Linux integration demo
- Portfolio / CV project

---

## Future Improvements
- Multi-device dashboard support
- Persistent database storage (SQLite)
- Alerts and notifications
- Authentication for dashboard access

---

## Author
Built as an educational and portfolio project to demonstrate real-world embedded + Linux system design.
