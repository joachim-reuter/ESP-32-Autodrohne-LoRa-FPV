#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ========== OLED ==========
Adafruit_SH1106G display(128, 64, &Wire, -1);

// ========== Trackball ==========
#define VRX_PIN 34
#define VRY_PIN 35

// ========== Encoder ==========
#define ENC_CLK 27
#define ENC_DT  14
#define ENC_SW  32

volatile int speed = 50;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ========== LoRa ==========
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   17
#define LORA_DIO0  26

int lastRSSI = -120;

// ========== Trackball Kalibrierung ==========
int xCenter = 0;
int yCenter = 0;
#define DEADZONE 120

// ---------- Encoder ISR ----------
void IRAM_ATTR encoderISR() {
  portENTER_CRITICAL_ISR(&mux);
  if (digitalRead(ENC_DT)) speed++;
  else speed--;
  speed = constrain(speed, 0, 100);
  portEXIT_CRITICAL_ISR(&mux);
}

// ---------- Trackball Mapping ----------
int mapAxis(int raw, int center) {
  if (abs(raw - center) < DEADZONE) return 0;

  if (raw > center) {
    return map(raw, center + DEADZONE, 4095, 0, 100);
  } else {
    return map(raw, 0, center - DEADZONE, -100, 0);
  }
}

// ---------- Kalibrierung ----------
void calibrateCenter() {
  long sx = 0, sy = 0;
  for (int i = 0; i < 50; i++) {
    sx += analogRead(VRX_PIN);
    sy += analogRead(VRY_PIN);
    delay(5);
  }
  xCenter = sx / 50;
  yCenter = sy / 50;
}

// ---------- RSSI Balken ----------
void drawRSSIBar(int rssi) {
  int bars = map(rssi, -120, -40, 0, 5);
  bars = constrain(bars, 0, 5);

  display.setCursor(70, 54);
  display.print("RSSI");

  for (int i = 0; i < 5; i++) {
    if (i < bars)
      display.fillRect(100 + i * 5, 58 - i * 4, 4, i * 4, SH110X_WHITE);
    else
      display.drawRect(100 + i * 5, 58 - i * 4, 4, i * 4, SH110X_WHITE);
  }
}

// ---------- OLED ----------
void drawOLED(int fb, int lr) {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("SPD ");
  display.print(speed);
  display.print("%");

  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("FB: ");
  display.print(fb);

  display.setCursor(0, 42);
  display.print("LR: ");
  display.print(lr);

  display.setCursor(0, 54);
  display.print(lastRSSI);
  display.print(" dBm");

  drawRSSIBar(lastRSSI);
  display.display();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, FALLING);

  Wire.begin(21, 22);
  display.begin(0x3C, true);
  display.setTextColor(SH110X_WHITE);

  // Trackball Kalibrieren (NICHT anfassen!)
  delay(1000);
  calibrateCenter();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  LoRa.begin(433E6);
}

// ---------- Loop ----------
void loop() {
  int rawX = analogRead(VRX_PIN);
  int rawY = analogRead(VRY_PIN);

  int lr = mapAxis(rawX, xCenter);
  int fb = -mapAxis(rawY, yCenter);

  String msg = "DRV," + String(fb) + "," + String(lr) + "," + String(speed);

  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();

  // ---- ACK / RSSI ----
  unsigned long t0 = millis();
  while (millis() - t0 < 30) {
    int p = LoRa.parsePacket();
    if (p) {
      String r = LoRa.readString();
      if (r.startsWith("ACK")) {
        lastRSSI = LoRa.packetRssi();
      }
    }
  }

  drawOLED(fb, lr);
  delay(40);
}