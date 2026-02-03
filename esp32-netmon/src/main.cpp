#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Ping.h>
#include <PubSubClient.h>

// ===== WIFI =====
const char* WIFI_SSID = "ChangeME";
const char* WIFI_PASS = "ChangeME";

// ===== MQTT CONFIG =====
const char* MQTT_HOST  = "192.168.33.13";
const int   MQTT_PORT  = 1883;
const char* MQTT_TOPIC = "netmon/esp32-1/metrics";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ===== LED PINS =====
const int LED_OK   = 16;  // green
const int LED_DEG  = 17;  // yellow
const int LED_DOWN = 18;  // red
const int LED_REC  = 19;  // blue (recovering)

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== TARGETS =====
IPAddress ROUTER_IP;                   // set from WiFi.gatewayIP()
IPAddress LINUX_IP(192, 168, 33, 13);  // change if needed

// ===== DEVICE STATE =====
enum DeviceState { STATE_OK, STATE_DEGRADED, STATE_DOWN, STATE_RECOVERING };

// ===== WiFi transition tracking =====
bool wasWifiConnected = false;
unsigned long wifiRecoveringUntilMs = 0;

// ===== Ping stats =====
struct PingStats {
  int lastMs = -1;
  bool lastOk = false;
  unsigned long lastOkMs = 0;   // when we last succeeded
};

PingStats routerStats;
PingStats linuxStats;

// ===== RTO tracking for Linux =====
bool linuxWasUp = true;
unsigned long linuxDownSinceMs = 0;
unsigned long showRtoUntilMs = 0;
int lastRtoSeconds = -1;

// ===== MQTT non-blocking reconnect =====
unsigned long lastMqttAttemptMs = 0;
const unsigned long MQTT_RETRY_MS = 3000;

// ===== Scheduling =====
const unsigned long SAMPLE_MS   = 2000; // ping/sample cadence
const unsigned long LCD_MS      = 500;  // LCD refresh cadence
const unsigned long WIFI_RETRY_MS = 3000;

unsigned long lastSampleMs = 0;
unsigned long lastLcdMs    = 0;
unsigned long lastWifiRetryMs = 0;

// ===== Last computed snapshot (what LCD shows) =====
DeviceState lastState = STATE_RECOVERING;
int lastRssi = 0;

// ===== LED helpers =====
void ledsOff() {
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_DEG, LOW);
  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_REC, LOW);
}

void setLeds(DeviceState s) {
  // Steady LEDs: OK / DEG / DOWN
  digitalWrite(LED_OK,   (s == STATE_OK)       ? HIGH : LOW);
  digitalWrite(LED_DEG,  (s == STATE_DEGRADED) ? HIGH : LOW);
  digitalWrite(LED_DOWN, (s == STATE_DOWN)     ? HIGH : LOW);
  // REC uses blinkRecovering()
}

void blinkRecovering() {
  static unsigned long last = 0;
  static bool on = false;
  if (millis() - last >= 250) {
    last = millis();
    on = !on;
    digitalWrite(LED_REC, on ? HIGH : LOW);
  }
}

void ledSelfTest() {
  digitalWrite(LED_OK, HIGH);   delay(150); digitalWrite(LED_OK, LOW);
  digitalWrite(LED_DEG, HIGH);  delay(150); digitalWrite(LED_DEG, LOW);
  digitalWrite(LED_DOWN, HIGH); delay(150); digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_REC, HIGH);  delay(150); digitalWrite(LED_REC, LOW);
}

// ===== LCD helpers =====
void lcdLine(int row, const String& text) {
  lcd.setCursor(0, row);
  String t = text;
  if (t.length() > 16) t = t.substring(0, 16);
  lcd.print(t);
  for (int i = t.length(); i < 16; i++) lcd.print(" ");
}

const char* stateToText(DeviceState s) {
  switch (s) {
    case STATE_OK: return "OK";
    case STATE_DEGRADED: return "DEG";
    case STATE_DOWN: return "DOWN";
    case STATE_RECOVERING: return "REC";
  }
  return "?";
}

// ===== WiFi helpers =====
bool isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void connectWifiBlocking() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  lcdLine(0, "Connecting WiFi");
  lcdLine(1, "Please wait...");

  Serial.print("Connecting to WiFi");
  while (!isWifiConnected()) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  ROUTER_IP = WiFi.gatewayIP();

  Serial.println("WiFi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("Gateway: "); Serial.println(ROUTER_IP);
  Serial.print("RSSI: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
}

// ===== MQTT non-blocking =====
void mqttEnsureConnectedNonBlocking(unsigned long now) {
  if (mqtt.connected()) return;
  if (now - lastMqttAttemptMs < MQTT_RETRY_MS) return;

  lastMqttAttemptMs = now;
  Serial.print("MQTT reconnect attempt... ");
  bool ok = mqtt.connect("esp32-netmon-1");
  Serial.println(ok ? "OK" : "FAIL");
}

// ===== Ping (NOTE: Ping.ping can block up to timeout when host is down) =====
void pingOnce(IPAddress ip, PingStats& st, unsigned long now) {
  bool ok = Ping.ping(ip, 1);
  int ms = ok ? (int)Ping.averageTime() : -1;

  st.lastOk = ok;
  st.lastMs = ms;
  if (ok) st.lastOkMs = now;
}

// ===== RTO logic =====
void updateLinuxRto(unsigned long now) {
  bool linuxUpNow = linuxStats.lastOk;

  if (linuxWasUp && !linuxUpNow) {
    linuxDownSinceMs = now;
    Serial.println("Linux DOWN start -> timer started");
  }

  if (!linuxWasUp && linuxUpNow) {
    unsigned long downTime = (linuxDownSinceMs > 0) ? (now - linuxDownSinceMs) : 0;
    lastRtoSeconds = (int)(downTime / 1000UL);
    showRtoUntilMs = now + 5000;

    Serial.print("Linux RECOVERED. RTO=");
    Serial.print(lastRtoSeconds);
    Serial.println("s");
  }

  linuxWasUp = linuxUpNow;
}

// ===== State logic (TIME-BASED, reacts fast and consistent) =====
const unsigned long ROUTER_DOWN_MS = 4000; // if router hasn't succeeded in 4s -> DOWN
const unsigned long LINUX_DOWN_MS  = 4000; // if linux hasn't succeeded in 4s  -> DEGRADED
const int HIGH_LAT_MS = 80;

DeviceState computeState(unsigned long now) {
  if (!isWifiConnected()) return STATE_DOWN;

  // Router is critical: if router is "stale" => DOWN
  if (routerStats.lastOkMs == 0 || (now - routerStats.lastOkMs) > ROUTER_DOWN_MS) {
    return STATE_DOWN;
  }

  // Linux is monitored service: if stale => DEGRADED (not full DOWN)
  if (linuxStats.lastOkMs == 0 || (now - linuxStats.lastOkMs) > LINUX_DOWN_MS) {
    return STATE_DEGRADED;
  }

  // Latency-based degrade
  if ((routerStats.lastOk && routerStats.lastMs >= HIGH_LAT_MS) ||
      (linuxStats.lastOk  && linuxStats.lastMs  >= HIGH_LAT_MS)) {
    return STATE_DEGRADED;
  }

  return STATE_OK;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_OK, OUTPUT);
  pinMode(LED_DEG, OUTPUT);
  pinMode(LED_DOWN, OUTPUT);
  pinMode(LED_REC, OUTPUT);
  ledsOff();

  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcdLine(0, "ESP32 NetMon");
  lcdLine(1, "Booting...");

  ledSelfTest();
  connectWifiBlocking();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  wasWifiConnected = true;
  wifiRecoveringUntilMs = millis() + 2000;

  // Initialize lastOkMs so we don't instantly mark DOWN before first samples
  unsigned long now = millis();
  routerStats.lastOkMs = now;
  linuxStats.lastOkMs  = now;
}

void loop() {
  unsigned long now = millis();
  bool connected = isWifiConnected();

  // Keep MQTT alive (never blocks)
  mqttEnsureConnectedNonBlocking(now);
  mqtt.loop();

  // Detect WiFi transition down->up
  if (!wasWifiConnected && connected) {
    Serial.println("WiFi reconnected -> recovering window");
    wifiRecoveringUntilMs = now + 5000;
  }
  wasWifiConnected = connected;

  // If WiFi is down: show DOWN immediately and retry connect occasionally
  if (!connected) {
    lastState = STATE_DOWN;
    setLeds(STATE_DOWN);
    digitalWrite(LED_REC, LOW);

    lcdLine(0, "WiFi DISCONNECTED");
    lcdLine(1, "ST: DOWN");

    if (now - lastWifiRetryMs > WIFI_RETRY_MS) {
      lastWifiRetryMs = now;
      WiFi.disconnect(true);
      delay(50);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    delay(10);
    return;
  }

  // === Sampling: run pings/state every 2 seconds ===
  if (now - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = now;

    // Ping Linux EVERY sample => fast reaction when Linux goes OFF
    pingOnce(LINUX_IP, linuxStats, now);

    // Ping router every other sample (still enough, reduces total blocking)
    static bool pingRouterToggle = false;
    pingRouterToggle = !pingRouterToggle;
    if (pingRouterToggle) {
      pingOnce(ROUTER_IP, routerStats, now);
    }

    updateLinuxRto(now);

    bool inRecovering = (now < wifiRecoveringUntilMs);
    lastRssi = WiFi.RSSI();

    if (now < showRtoUntilMs && lastRtoSeconds >= 0) {
      lastState = STATE_RECOVERING; // show REC while presenting RTO
    } else if (inRecovering) {
      lastState = STATE_RECOVERING;
    } else {
      lastState = computeState(now);
    }

    // Publish (only if MQTT up)
    if (mqtt.connected()) {
      String payload = "{";
      payload += "\"rssi\":" + String(lastRssi) + ",";
      payload += "\"router_ms\":" + String(routerStats.lastMs) + ",";
      payload += "\"linux_ms\":" + String(linuxStats.lastMs) + ",";
      payload += "\"state\":\"" + String(stateToText(lastState)) + "\"";
      payload += "}";
      mqtt.publish(MQTT_TOPIC, payload.c_str());
    }

    // Serial debug (once per sample)
    Serial.printf("R:%s | L:%s | RSSI:%d | state:%s | mqtt:%s\n",
                  routerStats.lastOk ? String(routerStats.lastMs).c_str() : "FAIL",
                  linuxStats.lastOk ? String(linuxStats.lastMs).c_str() : "FAIL",
                  lastRssi,
                  stateToText(lastState),
                  mqtt.connected() ? "UP" : "DOWN");
  }

  // === LEDs: update continuously (no waiting) ===
  if (lastState == STATE_RECOVERING) {
    // Turn off steady LEDs and blink REC
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_DEG, LOW);
    digitalWrite(LED_DOWN, LOW);
    blinkRecovering();
  } else {
    digitalWrite(LED_REC, LOW);
    setLeds(lastState);
  }

  // === LCD: refresh every 500ms using the latest snapshot ===
  if (now - lastLcdMs >= LCD_MS) {
    lastLcdMs = now;

    if (now < showRtoUntilMs && lastRtoSeconds >= 0) {
      lcdLine(0, "Linux RECOVERED");
      lcdLine(1, "RTO: " + String(lastRtoSeconds) + "s");
    } else {
      String line0 = "R:";
      line0 += routerStats.lastOk ? String(routerStats.lastMs) : "F";
      line0 += " L:";
      line0 += linuxStats.lastOk ? String(linuxStats.lastMs) : "F";
      line0 += " ";
      line0 += stateToText(lastState);

      lcdLine(0, line0);

      String mqttTxt = mqtt.connected() ? "MQTT:OK " : "MQTT:DN ";
      lcdLine(1, mqttTxt + "RSSI:" + String(lastRssi));
    }
  }

  delay(10); // keep loop responsive
}
