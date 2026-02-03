#!/usr/bin/env python3
import json
import os
import re
from collections import deque
from flask import Flask, jsonify, send_from_directory, request

LOG_DIR = os.getenv("NETMON_OUT_DIR", "/var/log/netmon")
LATEST_FILE = os.path.join(LOG_DIR, "latest.json")
LOG_FILE = os.path.join(LOG_DIR, "metrics.log")

app = Flask(__name__, static_folder="../dashboard", static_url_path="")

LINE_RE = re.compile(
    r'^(?P<ts>\S+)\s+device=(?P<device>\S+)\s+rssi=(?P<rssi>[^\s]+)\s+router_ms=(?P<router_ms>[^\s]+)\s+linux_ms=(?P<linux_ms>[^\s]+)\s+state=(?P<state>\S+)'
)

def read_latest():
  if not os.path.exists(LATEST_FILE):
    return {"error": "latest.json not found yet", "path": LATEST_FILE}
  try:
    with open(LATEST_FILE, "r", encoding="utf-8") as f:
      return json.load(f)
  except Exception as e:
    return {"error": str(e), "path": LATEST_FILE}

def parse_num(v):
  if v is None:
    return None
  s = str(v)
  if s in ("None", "-", ""):
    return None
  try:
    if "." in s:
      return float(s)
    return int(s)
  except Exception:
    return None

def read_history(n: int, device: str | None):
  """Return last n samples from metrics.log as JSON objects."""
  if not os.path.exists(LOG_FILE):
    return []

  # Read last n lines efficiently
  dq = deque(maxlen=max(1, n))
  try:
    with open(LOG_FILE, "r", encoding="utf-8", errors="replace") as f:
      for line in f:
        dq.append(line.rstrip("\n"))
  except Exception:
    return []

  out = []
  for line in dq:
    m = LINE_RE.match(line)
    if not m:
      continue
    obj = m.groupdict()
    if device and obj.get("device") != device:
      continue
    out.append({
      "ts": obj.get("ts"),
      "device": obj.get("device"),
      "rssi": parse_num(obj.get("rssi")),
      "router_ms": parse_num(obj.get("router_ms")),
      "linux_ms": parse_num(obj.get("linux_ms")),
      "state": obj.get("state"),
    })
  return out

@app.get("/api/latest")
def api_latest():
  return jsonify(read_latest())

@app.get("/api/history")
def api_history():
  # n = number of points to return
  try:
    n = int(request.args.get("n", "450"))  # default ~15 min at 2s
  except Exception:
    n = 450
  n = max(10, min(10000, n))

  device = request.args.get("device")
  if device == "":
    device = None

  return jsonify(read_history(n, device))

@app.get("/")
def index():
  return send_from_directory(app.static_folder, "index.html")

@app.get("/<path:path>")
def static_files(path):
  return send_from_directory(app.static_folder, path)

if __name__ == "__main__":
  host = os.getenv("HOST", "0.0.0.0")
  port = int(os.getenv("PORT", "8080"))
  app.run(host=host, port=port)
