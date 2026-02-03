#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/opt/netmon"
LOG_DIR="/var/log/netmon"
SERVICE="netmon-collector.service"

PURGE=false
if [[ "${1:-}" == "--purge" ]]; then
  PURGE=true
fi

echo "[1/6] Stopping and disabling service..."
sudo systemctl stop "$SERVICE" || true
sudo systemctl disable "$SERVICE" || true

echo "[2/6] Removing systemd unit..."
sudo rm -f "/etc/systemd/system/$SERVICE"
sudo systemctl daemon-reload

echo "[3/6] Removing logrotate config..."
sudo rm -f /etc/logrotate.d/netmon

echo "[4/6] (Optional) Removing app directory..."
if $PURGE; then
  sudo rm -rf "$APP_DIR"
  echo "Removed $APP_DIR"
else
  echo "Kept $APP_DIR (use --purge to remove)"
fi

echo "[5/6] (Optional) Removing log directory..."
if $PURGE; then
  sudo rm -rf "$LOG_DIR"
  echo "Removed $LOG_DIR"
else
  echo "Kept $LOG_DIR (use --purge to remove)"
fi

echo "[6/6] Uninstall complete."

