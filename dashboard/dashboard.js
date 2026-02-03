// Dashboard refresh intervals
const LATEST_MS = 2000;   // /api/latest polling
const HISTORY_MS = 5000;  // /api/history polling

let chartRssi;
let chartLatency;

function fmtTime(isoUtc) {
  // "2026-02-03T04:12:30Z" -> "04:12:30"
  if (!isoUtc || typeof isoUtc !== "string") return "";
  const t = isoUtc.split("T")[1] || "";
  return t.replace("Z", "").slice(0, 8);
}

function ensureCharts() {
  if (chartRssi && chartLatency) return;

  const ctxR = document.getElementById("chartRssi");
  const ctxL = document.getElementById("chartLatency");

  chartRssi = new Chart(ctxR, {
    type: "line",
    data: { labels: [], datasets: [{ label: "RSSI (dBm)", data: [], tension: 0.2 }] },
    options: {
      responsive: true,
      animation: false,
      plugins: { legend: { display: true } },
      scales: {
        x: { ticks: { maxRotation: 0, autoSkip: true } },
        y: { title: { display: true, text: "dBm" } }
      }
    }
  });

  chartLatency = new Chart(ctxL, {
    type: "line",
    data: {
      labels: [],
      datasets: [
        { label: "Router (ms)", data: [], tension: 0.2 },
        { label: "Linux (ms)", data: [], tension: 0.2 }
      ]
    },
    options: {
      responsive: true,
      animation: false,
      plugins: { legend: { display: true } },
      scales: {
        x: { ticks: { maxRotation: 0, autoSkip: true } },
        y: { title: { display: true, text: "ms" }, beginAtZero: true }
      }
    }
  });
}

function setStateClass(stateEl, state) {
  stateEl.classList.remove("ok", "deg", "down", "recovering");
  if (state === "OK") stateEl.classList.add("ok");
  else if (state === "DEGRADED") stateEl.classList.add("deg");
  else if (state === "DOWN") stateEl.classList.add("down");
  else if (state === "RECOVERING") stateEl.classList.add("recovering");
}

async function refreshLatest() {
  try {
    const res = await fetch("/api/latest", { cache: "no-store" });
    const data = await res.json();

    const stateEl = document.getElementById("state");
    const rssiEl = document.getElementById("rssi");
    const routerEl = document.getElementById("router");
    const linuxEl = document.getElementById("linux");
    const deviceEl = document.getElementById("device");
    const tsEl = document.getElementById("ts");

    const state = (data.state || "-").toUpperCase();

    stateEl.textContent = state;
    rssiEl.textContent = data.rssi ?? "-";
    routerEl.textContent = data.router_ms ?? "-";
    linuxEl.textContent = data.linux_ms ?? "-";
    deviceEl.textContent = data.device ?? "-";
    tsEl.textContent = data.ts ?? "-";

    setStateClass(stateEl, state);

    // If device dropdown is empty and we got a device, set it
    const deviceSelect = document.getElementById("deviceSelect");
    if (data.device && deviceSelect && deviceSelect.options.length <= 1) {
      const opt = document.createElement("option");
      opt.value = data.device;
      opt.textContent = data.device;
      deviceSelect.appendChild(opt);
    }
  } catch (e) {
    console.error("latest error", e);
  }
}

function computeN(minutes) {
  // Default collector cadence ~2s, so ~30 points/min
  const perMin = 30;
  return Math.max(30, minutes * perMin);
}

async function refreshHistory() {
  ensureCharts();
  const minutes = parseInt(document.getElementById("window").value, 10);
  const n = computeN(minutes);

  const deviceSelect = document.getElementById("deviceSelect");
  const selectedDevice = deviceSelect ? deviceSelect.value : "";
  const qs = new URLSearchParams({ n: String(n) });
  if (selectedDevice) qs.set("device", selectedDevice);

  try {
    const res = await fetch(`/api/history?${qs.toString()}`, { cache: "no-store" });
    const arr = await res.json();

    // Expect arr = [{ts, device, rssi, router_ms, linux_ms, state}, ...]
    const labels = (arr || []).map(p => fmtTime(p.ts));
    const rssi = (arr || []).map(p => (p.rssi ?? null));
    const router = (arr || []).map(p => (p.router_ms ?? null));
    const linux = (arr || []).map(p => (p.linux_ms ?? null));

    chartRssi.data.labels = labels;
    chartRssi.data.datasets[0].data = rssi;
    chartRssi.update();

    chartLatency.data.labels = labels;
    chartLatency.data.datasets[0].data = router;
    chartLatency.data.datasets[1].data = linux;
    chartLatency.update();

    // Populate device dropdown if multiple devices appear
    const devices = new Set((arr || []).map(p => p.device).filter(Boolean));
    if (deviceSelect && devices.size > 0) {
      const existing = new Set(Array.from(deviceSelect.options).map(o => o.value).filter(Boolean));
      for (const d of devices) {
        if (!existing.has(d)) {
          const opt = document.createElement("option");
          opt.value = d;
          opt.textContent = d;
          deviceSelect.appendChild(opt);
        }
      }
    }
  } catch (e) {
    console.error("history error", e);
  }
}

function boot() {
  refreshLatest();
  refreshHistory();

  setInterval(refreshLatest, LATEST_MS);
  setInterval(refreshHistory, HISTORY_MS);

  // Refresh history immediately when controls change
  document.getElementById("window").addEventListener("change", refreshHistory);
  document.getElementById("deviceSelect").addEventListener("change", refreshHistory);
}

boot();
