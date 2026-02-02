#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Ping.h>
#include <PubSubClient.h>

// ===== WIFI CONFIG =====
const char* WIFI_SSID = "ChangeME";
const char* WIFI_PASS = "ChangeME";

// ===== MQTT CONFIG =====
const char* MQTT_HOST = "192.168.33.13";
const int   MQTT_PORT = 1883;
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
IPAddress LINUX_IP(192, 168, 33, 13);  // <-- CHANGE if needed

// ===== DEVICE STATE =====
enum DeviceState { STATE_OK, STATE_DEGRADED, STATE_DOWN, STATE_RECOVERING };

// ===== WiFi transition tracking =====
bool wasWifiConnected = false;
unsigned long wifiRecoveringUntilMs = 0;

// ===== Ping stats =====
struct PingStats {
  int lastMs = -1;
  bool lastOk = false;
  int consecFails = 0;
};

PingStats routerStats;
PingStats linuxStats;

// ===== RTO tracking for Linux =====
bool linuxWasUp = true;                 // previous "linux reachable" status
unsigned long linuxDownSinceMs = 0;      // when linux first went down
unsigned long showRtoUntilMs = 0;        // while this is active, show REC screen
int lastRtoSeconds = -1;                // last computed RTO seconds

// ===== LED helpers =====
void ledsOff() {
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_DEG, LOW);
  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_REC, LOW);
}

void setLeds(DeviceState s) {
  // NOTE: during RTO/REC screen we override LEDs, so this is "normal mode"
  ledsOff();
  if (s == STATE_OK) digitalWrite(LED_OK, HIGH);
  else if (s == STATE_DEGRADED) digitalWrite(LED_DEG, HIGH);
  else if (s == STATE_DOWN) digitalWrite(LED_DOWN, HIGH);
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
  digitalWrite(LED_OK, HIGH);   delay(200); digitalWrite(LED_OK, LOW);
  digitalWrite(LED_DEG, HIGH);  delay(200); digitalWrite(LED_DEG, LOW);
  digitalWrite(LED_DOWN, HIGH); delay(200); digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_REC, HIGH);  delay(200); digitalWrite(LED_REC, LOW);
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
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  ROUTER_IP = WiFi.gatewayIP();

  Serial.println("WiFi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("Gateway: "); Serial.println(ROUTER_IP);
  Serial.print("RSSI: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect("esp32-netmon-1")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// ===== Ping =====
void pingOnce(IPAddress ip, PingStats& st) {
  bool ok = Ping.ping(ip, 1);
  int ms = ok ? (int)Ping.averageTime() : -1;

  st.lastOk = ok;
  st.lastMs = ms;

  if (!ok) st.consecFails++;
  else st.consecFails = 0;
}

// ===== State logic =====
// Worst target decides state.
DeviceState computeState() {
  if (!isWifiConnected()) return STATE_DOWN;

  // Router down => system down
  if (routerStats.consecFails >= 3) return STATE_DOWN;

  // Linux down => degraded (service unavailable)
  if (linuxStats.consecFails >= 3) return STATE_DEGRADED;

  // High latency => degraded
  if ((routerStats.lastOk && routerStats.lastMs >= 80) ||
      (linuxStats.lastOk && linuxStats.lastMs >= 80)) {
    return STATE_DEGRADED;
  }

  // No data yet
  if (routerStats.lastMs < 0 || linuxStats.lastMs < 0) return STATE_RECOVERING;

  return STATE_OK;
}

// ===== RTO logic =====
void updateLinuxRto(unsigned long now) {
  // Current linux reachable? (we use lastOk)
  bool linuxUpNow = linuxStats.lastOk;

  // If it just went DOWN, mark the time (only once)
  if (linuxWasUp && !linuxUpNow) {
    linuxDownSinceMs = now;
    Serial.println("Linux DOWN start -> timer started");
  }

  // If it just RECOVERED (was down, now up), compute RTO and show it
  if (!linuxWasUp && linuxUpNow) {
    unsigned long downTime = (linuxDownSinceMs > 0) ? (now - linuxDownSinceMs) : 0;
    lastRtoSeconds = (int)(downTime / 1000UL);

    // show REC screen for 5 seconds
    showRtoUntilMs = now + 5000;

    Serial.print("Linux RECOVERED. RTO=");
    Serial.print(lastRtoSeconds);
    Serial.println("s");
  }

  linuxWasUp = linuxUpNow;
}

void setup() {
  Serial.begin(115200);
  delay(300);

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
  connectMqtt();

  wasWifiConnected = true;
  wifiRecoveringUntilMs = millis() + 2000;
}

void loop() {
  unsigned long now = millis();
  bool connected = isWifiConnected();

  // WiFi transition: down -> up
  if (!wasWifiConnected && connected) {
    Serial.println("WiFi reconnected -> recovering window");
    wifiRecoveringUntilMs = now + 5000;
  }
  wasWifiConnected = connected;

  // If WiFi is down, try reconnect and show DOWN
  if (!connected) {
    setLeds(STATE_DOWN);
    lcdLine(0, "WiFi DISCONNECTED");
    lcdLine(1, "ST: DOWN");

    WiFi.disconnect(true);
    delay(200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(1000);
    return;
  }

  // Ping router + linux
  pingOnce(ROUTER_IP, routerStats);
  pingOnce(LINUX_IP,  linuxStats);

  // Update RTO tracking based on Linux up/down transitions
  updateLinuxRto(now);

  DeviceState s = computeState();

  // ===== Priority 1: Show RTO recovery screen =====
  if (now < showRtoUntilMs && lastRtoSeconds >= 0) {
    // Blue blink while showing REC time
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_DEG, LOW);
    digitalWrite(LED_DOWN, LOW);
    blinkRecovering();

    lcdLine(0, "Linux RECOVERED");
    lcdLine(1, "RTO: " + String(lastRtoSeconds) + "s");

    delay(500);
    return;
  }

  // ===== Priority 2: WiFi recovering window =====
  if (now < wifiRecoveringUntilMs) {
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_DEG, LOW);
    digitalWrite(LED_DOWN, LOW);
    blinkRecovering();
    s = STATE_RECOVERING;
  } else {
    setLeds(s);
    digitalWrite(LED_REC, LOW);
  }

  // LCD normal screen
  String line0 = "R:";
  line0 += routerStats.lastOk ? String(routerStats.lastMs) : "F";
  line0 += " L:";
  line0 += linuxStats.lastOk ? String(linuxStats.lastMs) : "F";
  line0 += " ";
  line0 += stateToText(s);

  lcdLine(0, line0);
  lcdLine(1, "RSSI:" + String(WiFi.RSSI()) + " dBm");

  // Serial debug
  Serial.printf("R:%s (%d fails) | L:%s (%d fails) | RSSI:%d | state:%s\n",
                routerStats.lastOk ? String(routerStats.lastMs).c_str() : "FAIL",
                routerStats.consecFails,
                linuxStats.lastOk ? String(linuxStats.lastMs).c_str() : "FAIL",
                linuxStats.consecFails,
                WiFi.RSSI(),
                stateToText(s));

  if (!mqtt.connected()) {
  connectMqtt();
}
mqtt.loop();

// Build JSON message
String payload = "{";
payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
payload += "\"router_ms\":" + String(routerStats.lastMs) + ",";
payload += "\"linux_ms\":" + String(linuxStats.lastMs) + ",";
payload += "\"state\":\"" + String(stateToText(s)) + "\"";
payload += "}";

// Publish
mqtt.publish(MQTT_TOPIC, payload.c_str());

  delay(2000);
}
