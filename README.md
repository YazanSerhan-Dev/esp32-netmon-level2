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

**Deployment note:** You can run the backend either **natively on Linux (systemd)** or via **Docker Compose** (recommended for quick setup). Mosquitto can be native or containerized.

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

## Quick Start (Docker Compose – Option A)

This mode runs **collector + web** in Docker, while Mosquitto runs on the **host Linux** (so you avoid port conflicts and keep it simple).

### 0) Prereqs on Linux
- Docker + docker-compose (or `docker compose` plugin)
- Mosquitto broker running on the host (port **1883**)

### 1) Start the stack
From the repo root:

```bash
docker-compose down --remove-orphans
docker-compose up -d --build
docker ps
```

### 2) Open the dashboard
- `http://localhost:8080`

### 3) Where data is stored
Docker volume `netmon_data` is mounted into both containers at `/data`:
- `/data/latest.json`
- `/data/metrics.log`

---

## Native Linux Mode (systemd)

If you prefer the "production-like" setup:
- Mosquitto runs as a Linux service
- `server/collector.py` runs as a **systemd service**
- Web server runs either as a service or manually

See the `linux/` folder for service files and deployment notes.

---

## API Endpoints

- `GET /api/latest` → latest snapshot (`latest.json`)
- `GET /api/history?n=450&device=esp32-1` → last N samples parsed from `metrics.log`

---

---

## Engineering Principles
- Non-blocking embedded design
- Time-based state detection (not failure counters)
- Clear separation between runtime data and source code
- Linux-standard filesystem layout
- Production-style logging and service management

---

## Use Cases
- Embedded systems learning
- Network health monitoring
- IoT + Linux integration demo
- Portfolio / CV project

---

## Future Improvements
- Containerized Mosquitto (Option B)
- Better multi-device UX in dashboard (auto-discovery)
- Persistent database storage (SQLite)
- Alerts and notifications
- Authentication for dashboard access

---

## Author
Built as an educational and portfolio project to demonstrate real-world embedded + Linux system design.
