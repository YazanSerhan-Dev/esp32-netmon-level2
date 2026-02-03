#!/usr/bin/env python3
import json
import os
from flask import Flask, jsonify, send_from_directory

LOG_DIR = os.getenv("NETMON_OUT_DIR", "/var/log/netmon")
LATEST_FILE = os.path.join(LOG_DIR, "latest.json")

app = Flask(__name__, static_folder="../dashboard", static_url_path="")

def read_latest():
  if not os.path.exists(LATEST_FILE):
    return {"error": "latest.json not found yet", "path": LATEST_FILE}
  try:
    with open(LATEST_FILE, "r", encoding="utf-8") as f:
      return json.load(f)
  except Exception as e:
    return {"error": str(e), "path": LATEST_FILE}

@app.get("/api/latest")
def api_latest():
  return jsonify(read_latest())

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
