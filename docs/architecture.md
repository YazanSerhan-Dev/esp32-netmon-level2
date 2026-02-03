# ESP32 NetMon – System Architecture

## 1. Overview
ESP32 NetMon is a small distributed monitoring system that measures network health using an ESP32 device and a Linux-based backend.
The project demonstrates real-world concepts from embedded systems, networking, Linux services, logging, and system observability.

The system is intentionally split into clear layers to reflect how production monitoring systems are built.

---

## 2. High-Level Architecture

The system consists of four main layers:

1. Embedded device layer (ESP32)
2. Network and messaging layer (WiFi + MQTT)
3. Linux backend layer (MQTT broker + collector service)
4. Persistence and observability layer (logs and state files)

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
ESP32 devices publish metrics to the following topic structure:

```
netmon/<device-id>/metrics
```

This design allows multiple devices to be monitored simultaneously.

### Data Format
Messages are sent as JSON objects containing:
- RSSI value
- Router latency (milliseconds)
- Linux server latency (milliseconds)
- Current device state

---

## 5. Linux Backend Layer

### MQTT Broker
- Mosquitto MQTT broker runs on the Linux machine
- Receives messages from ESP32 devices
- Distributes messages to subscribers

### Collector Service
- Implemented in Python (`collector.py`)
- Runs as a `systemd` service
- Subscribes to `netmon/+/metrics`
- Parses incoming JSON messages
- Maintains the latest known device state
- Writes structured logs to disk

---

## 6. Persistence & Logging

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

## 7. Deployment Model

- ESP32 firmware is built and flashed using PlatformIO
- Linux services are installed under `/opt/netmon`
- The collector runs as a managed `systemd` service
- GitHub stores source code and configuration only
- Runtime data (logs, state files) remain local to the Linux system

---

## 8. Design Goals

- Clear separation between embedded, network, and backend layers
- Real-world Linux filesystem and service layout
- Fault tolerance via service supervision and log rotation
- Extensible design supporting multiple devices and dashboards

---

## 9. Future Extensions

- Web-based dashboard for live visualization
- Threshold-based alerts
- Support for multiple ESP32 devices
- Persistent database storage for historical analysis

