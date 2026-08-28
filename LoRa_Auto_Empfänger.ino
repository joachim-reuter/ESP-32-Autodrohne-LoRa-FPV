#include <SPI.h>
#include <LoRa.h>

// ================= LoRa =================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   -1
#define LORA_DIO0  26

// ================= DRV8833 =================
// Linke Seite (2 Motoren parallel)
#define M1_PWM 25
#define M1_IN1 27
#define M1_IN2 14

// Rechte Seite (2 Motoren parallel)
#define M2_PWM 33
#define M2_IN1 12
#define M2_IN2 13

#define DRV_STBY 32

// ================= Failsafe =================
unsigned long lastPacket = 0;
const unsigned long FAILSAFE_MS = 400;

// ================= Motor =================
void setMotor(int pwmPin, int in1, int in2, int value) {
  value = constrain(value, -255, 255);

  if (value > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, value);
  }
  else if (value < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(pwmPin, -value);
  }
  else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, 0);
  }
}

void stopMotors() {
  analogWrite(M1_PWM, 0);
  analogWrite(M2_PWM, 0);

  digitalWrite(M1_IN1, LOW);
  digitalWrite(M1_IN2, LOW);
  digitalWrite(M2_IN1, LOW);
  digitalWrite(M2_IN2, LOW);
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  Serial.println("🚗 LoRa EMPFÄNGER gestartet");

  pinMode(M1_PWM, OUTPUT);
  pinMode(M1_IN1, OUTPUT);
  pinMode(M1_IN2, OUTPUT);

  pinMode(M2_PWM, OUTPUT);
  pinMode(M2_IN1, OUTPUT);
  pinMode(M2_IN2, OUTPUT);

  pinMode(DRV_STBY, OUTPUT);
  digitalWrite(DRV_STBY, HIGH);

  stopMotors();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa Fehler");
    while (1);
  }

  Serial.println("✅ LoRa bereit");
}

// ================= Loop =================
void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String msg = LoRa.readString();
    msg.trim();
    lastPacket = millis();

    int fb, lr, speed;
    if (sscanf(msg.c_str(), "DRV,%d,%d,%d", &fb, &lr, &speed) == 3) {

      // --- Mischen ---
      int left  = fb + lr;
      int right = fb - lr;

      left  = constrain(left,  -100, 100);
      right = constrain(right, -100, 100);

      int maxPWM = map(speed, 0, 100, 0, 255);

      left  = left  * maxPWM / 100;
      right = right * maxPWM / 100;

      setMotor(M1_PWM, M1_IN1, M1_IN2, left);
      setMotor(M2_PWM, M2_IN1, M2_IN2, right);

      // ACK (optional für RSSI)
      LoRa.beginPacket();
      LoRa.print("ACK");
      LoRa.endPacket();
    }
  }

  // ================= Failsafe =================
  if (millis() - lastPacket > FAILSAFE_MS) {
    stopMotors();
    digitalWrite(DRV_STBY, LOW);
  } else {
    digitalWrite(DRV_STBY, HIGH);
  }
}
