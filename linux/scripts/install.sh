#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/opt/netmon"
LOG_DIR="/var/log/netmon"
SERVICE="netmon-collector.service"

echo "[1/7] Creating directories..."
sudo mkdir -p "$APP_DIR" "$LOG_DIR"

echo "[2/7] Copying server code..."
sudo rsync -a --delete server/ "$APP_DIR/"

echo "[3/7] Setting permissions..."
sudo chown -R netmon:netmon "$APP_DIR" "$LOG_DIR"

echo "[4/7] Installing systemd service..."
sudo cp linux/systemd/$SERVICE /etc/systemd/system/$SERVICE
sudo systemctl daemon-reload

echo "[5/7] Enabling and starting service..."
sudo systemctl enable --now $SERVICE

echo "[6/7] Installing logrotate config..."
sudo cp linux/logrotate/netmon /etc/logrotate.d/netmon

echo "[7/7] Verifying status..."
sudo systemctl status $SERVICE --no-pager
