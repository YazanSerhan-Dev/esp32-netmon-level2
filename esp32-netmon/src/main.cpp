#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===== WIFI CONFIG =====
const char* WIFI_SSID = "ChangeME";
const char* WIFI_PASS = "ChangeME";

// ===== LED PINS =====
const int LED_OK   = 16;  // green
const int LED_DEG  = 17;  // yellow (reserved for later)
const int LED_DOWN = 18;  // red
const int LED_REC  = 19;  // blue (recovering)

// ===== LCD (I2C) =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== DEVICE STATE =====
enum DeviceState { STATE_OK, STATE_DEGRADED, STATE_DOWN, STATE_RECOVERING };

// Track connection transitions to show RECOVERING blink
bool wasConnected = false;
unsigned long recoveringUntilMs = 0;

// ===== LED HELPERS =====
void ledsOff() {
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_DEG, LOW);
  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_REC, LOW);
}

void setLeds(DeviceState s) {
  ledsOff();

  if (s == STATE_OK) {
    digitalWrite(LED_OK, HIGH);
  } else if (s == STATE_DEGRADED) {
    digitalWrite(LED_DEG, HIGH);
  } else if (s == STATE_DOWN) {
    digitalWrite(LED_DOWN, HIGH);
  }
  // STATE_RECOVERING handled by blinking function
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

// ===== LCD HELPERS =====
const char* stateToText(DeviceState s) {
  switch (s) {
    case STATE_OK: return "OK";
    case STATE_DEGRADED: return "DEGRADED";
    case STATE_DOWN: return "DOWN";
    case STATE_RECOVERING: return "RECOVERING";
  }
  return "?";
}

// Pads/clears a full 16-char line
void lcdLine(int row, const String& text) {
  lcd.setCursor(0, row);
  String t = text;
  if (t.length() > 16) t = t.substring(0, 16);
  lcd.print(t);
  for (int i = t.length(); i < 16; i++) lcd.print(" ");
}

void updateLcd(DeviceState s) {
  // Line 0: WiFi + RSSI
  if (WiFi.status() == WL_CONNECTED) {
    lcdLine(0, "WiFi OK RSSI " + String(WiFi.RSSI()));
  } else {
    lcdLine(0, "WiFi DISCONNECTED");
  }

  // Line 1: State + IP (short)
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    lcdLine(1, String("ST: ") + stateToText(s) + " " + ipStr);
  } else {
    lcdLine(1, String("ST: ") + stateToText(s));
  }
}

// ===== WIFI HELPERS =====
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

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // Init LEDs
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_DEG, OUTPUT);
  pinMode(LED_DOWN, OUTPUT);
  pinMode(LED_REC, OUTPUT);
  ledsOff();

  // Init LCD
  Wire.begin();           // ESP32 uses default I2C pins (usually 21 SDA, 22 SCL)
  lcd.init();
  lcd.backlight();
  lcdLine(0, "ESP32 NetMon");
  lcdLine(1, "Booting...");

  Serial.println();
  Serial.println("ESP32 NetMon starting...");

  // LED test
  ledSelfTest();

  // WiFi
  connectWifiBlocking();

  wasConnected = true;
  recoveringUntilMs = millis() + 2000; // brief recovering after boot
}

void loop() {
  const unsigned long now = millis();
  const bool connected = isWifiConnected();

  // Transition: DOWN -> connected
  if (!wasConnected && connected) {
    Serial.println("WiFi reconnected -> RECOVERING");
    recoveringUntilMs = now + 5000;
  }
  wasConnected = connected;

  // If disconnected, try reconnect
  if (!connected) {
    Serial.println("WiFi disconnected! Reconnecting...");
    setLeds(STATE_DOWN);
    updateLcd(STATE_DOWN);

    WiFi.disconnect(true);
    delay(200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    delay(1000);
    return;
  }

  // If in recovering window, blink blue
  if (now < recoveringUntilMs) {
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_DEG, LOW);
    digitalWrite(LED_DOWN, LOW);
    blinkRecovering();
    updateLcd(STATE_RECOVERING);
  } else {
    setLeds(STATE_OK);
    digitalWrite(LED_REC, LOW);
    updateLcd(STATE_OK);
  }

  // Serial output
  Serial.print("WiFi status: ");
  Serial.print(WiFi.status());
  Serial.print(" | RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  delay(2000);
}
