async function refresh() {
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

    stateEl.classList.remove("ok","deg","down","rec");
    if (state === "OK") stateEl.classList.add("ok");
    else if (state === "DEG" || state === "DEGRADED") stateEl.classList.add("deg");
    else if (state === "DOWN") stateEl.classList.add("down");
    else if (state === "REC" || state === "RECOVERING") stateEl.classList.add("rec");

  } catch (e) {
    document.getElementById("state").textContent = "NO DATA";
  }
}

refresh();
setInterval(refresh, 2000);
