# ESP32 NetMon – System Architecture

## 1. Overview
ESP32 NetMon is a small distributed monitoring system that measures network health using an ESP32 device and a Linux-based backend.
The project demonstrates real-world concepts from embedded systems, networking, Linux services, logging, containerization (Docker), and basic system observability.

The system is intentionally split into clear layers to reflect how production monitoring systems are built.

---

## 2. High-Level Architecture

The system consists of five main layers:

1. Embedded device layer (ESP32)
2. Network and messaging layer (WiFi + MQTT)
3. Linux messaging layer (Mosquitto broker)
4. Backend + API layer (collector + web API + dashboard)
5. Persistence and observability layer (logs and state files)

---

## 3. Embedded Layer (ESP32)

### Responsibilities
- Connect to a WiFi network
- Measure network metrics:
  - RSSI (WiFi signal strength)
  - Latency to the local router
  - Latency to the Linux server
- Determine the current device state (OK / DEGRADED / DOWN / RECOVERING)
- Publish metrics periodically using MQTT
- Display the current state using LEDs and an I2C LCD

### Hardware Interfaces
- GPIO LEDs (GPIO 16–19) for state indication
- I2C LCD (16x2) for textual status output
- WiFi radio for network communication

---

## 4. Network & Messaging Layer

### Protocols Used
- WiFi (IEEE 802.11)
- MQTT (publish/subscribe messaging)

### MQTT Topics
The ESP32 publishes metrics to a single-device topic:

```
netmon/esp32-1/metrics
```

> Note: The topic naming style is compatible with multi-device expansion, but the current project deployment targets a single ESP32 device.

### Data Format
Messages are sent as JSON objects containing:
- RSSI value
- Router latency (milliseconds)
- Linux server latency (milliseconds)
- Current device state

---

## 5. Linux Messaging Layer (MQTT Broker)

### MQTT Broker
- Mosquitto MQTT broker runs on the Linux machine
- Receives messages from the ESP32
- Distributes messages to subscribers (collector)

---

## 6. Backend + API Layer

### Collector Service
- Implemented in Python (`collector.py`)
- Subscribes to:

```
netmon/esp32-1/metrics
```

- Parses incoming JSON messages
- Maintains the latest known device state
- Writes structured logs and state snapshots to disk

### Web/API Service + Dashboard
- Implemented in Python (`app.py`) using Flask
- Exposes REST endpoints used by the dashboard:
  - `GET /api/latest` – latest state snapshot
  - `GET /api/history?n=<N>` – last N samples parsed from `metrics.log`
- Serves the web dashboard (static files) from `/` for live visualization (graphs + current status)

---

## 7. Persistence & Logging

### Log Files
- Metrics are written to:

```
/var/log/netmon/metrics.log
```

### State File
- The most recent device state snapshot is stored in:

```
/var/log/netmon/latest.json
```

### Log Rotation
- Log rotation is handled using `logrotate`
- Prevents unlimited log growth
- Old logs are rotated and automatically removed

---

## 8. Deployment Model

ESP32:
- Firmware is built and flashed using PlatformIO

Linux (current deployment):
- Mosquitto runs on the Linux host (system service)
- Collector + Web/API + Dashboard run using Docker Compose:
  - `netmon-collector` container (Python MQTT subscriber)
  - `netmon-web` container (Flask API + dashboard)
- Containers share a Docker volume for runtime data (logs/state)
- GitHub stores source code and configuration only
- Runtime data (logs, state files) remain local to the Linux system

> Alternative mode: the collector can also run as a managed `systemd` service (used earlier in the project). The Docker deployment is the preferred “one-command” runtime.

---

## 9. Design Goals

- Clear separation between embedded, network, and backend layers
- Real-world Linux filesystem layout for observability data
- Fault tolerance via service supervision / container restart policies and log rotation
- Simple, single-device deployment that still leaves room for future scaling
- Live visualization through an HTTP dashboard backed by real collected metrics

---

## 10. Future Extensions

- Threshold-based alerts (e.g., notify if DOWN persists > X seconds)
- Multi-device support (topic wildcards + per-device storage)
- Persistent database storage for historical analysis (SQLite/PostgreSQL)
- Hardened MQTT security (authentication/ACLs + TLS)
