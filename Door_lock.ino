#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// Servo setup
Servo myServo;
const int servoPin = 13;        // D13 (GPIO13) for servo
const int minPulseWidth = 500;  // 0.5 ms for 0 degrees
const int maxPulseWidth = 2500; // 2.5 ms for 180 degrees

// RFID setup
#define RST_PIN 22              // D22 (GPIO22) for RFID reset
#define SS_PIN 18               // D18 (GPIO18) for RFID SS
MFRC522 mfrc522(SS_PIN, RST_PIN);

// LED and Buzzer setup
#define LED_PIN 15              // D15 (GPIO15) for LED
#define BUZZER_PIN 14           // D14 (GPIO14) for buzzer

// Authorized UID (change to your card's UID)
byte authorizedUID[] = {0xEF, 0x27, 0x1E, 0x1E};

void setup() {
  Serial.begin(115200);         // Start Serial at 115200 baud
  delay(2000);                  // Wait for Serial to stabilize

  // Initialize SPI and RFID
  SPI.begin(19, 21, 23, SS_PIN); // SPI: SCK=D19, MISO=D21, MOSI=D23, SS=D18
  mfrc522.PCD_Init();
  Serial.println("===================");
  Serial.println("ESP32 + RFID + Servo + LED + Buzzer Access Control");
  Serial.println("===================");

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // Start with LED OFF
  Serial.println("✓ LED initialized (OFF)");

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Start with buzzer OFF
  Serial.println("✓ Buzzer initialized (OFF)");

  // Initialize Servo
  myServo.attach(servoPin, minPulseWidth, maxPulseWidth);
  myServo.setPeriodHertz(50);   // 50Hz for S3003
  myServo.writeMicroseconds(map(0, 0, 180, minPulseWidth, maxPulseWidth)); // Start at 0° (vertical, closed)
  Serial.println("✓ Servo initialized at 0 degrees (vertical, closed)");

  Serial.println("System Ready. Place your card...");

  // Startup indicator - blink LED and beep buzzer 3 times
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH); // Short beep
    delay(200);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void loop() {
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Print UID
  Serial.print("Card UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check if UID matches authorized card
  if (isAuthorized(mfrc522.uid.uidByte, mfrc522.uid.size)) {
    Serial.println("✅ Authorized card detected! ACCESS GRANTED");
    grantAccess();
  } else {
    Serial.println("❌ Unauthorized card! ACCESS DENIED");
    denyAccess();
  }

  mfrc522.PICC_HaltA();
  delay(1000); // Prevent rapid re-reading
}

// Check if UID matches authorized card
bool isAuthorized(byte *uid, byte uidSize) {
  if (uidSize != sizeof(authorizedUID)) return false;
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}

// Grant access - LED ON, long buzzer beep, servo to 90° for 5 seconds
void grantAccess() {
  Serial.println("🟢 LED ON, Buzzer long beep, Servo to 90° (horizontal, open)");
  digitalWrite(LED_PIN, HIGH);    // Turn LED ON
  digitalWrite(BUZZER_PIN, HIGH); // Start long beep
  int pulseWidth = map(90, 0, 180, minPulseWidth, maxPulseWidth); // Map 90° to ~1500µs
  myServo.writeMicroseconds(pulseWidth); // Move to 90° (horizontal, open)
  
  delay(5000); // Keep LED ON, buzzer beeping, and servo at 90° for 5 seconds
  
  digitalWrite(LED_PIN, LOW);    // Turn LED OFF
  digitalWrite(BUZZER_PIN, LOW); // Stop buzzer
  pulseWidth = map(0, 0, 180, minPulseWidth, maxPulseWidth); // Map 0° to ~500µs
  myServo.writeMicroseconds(pulseWidth); // Return to 0° (vertical, closed)
  Serial.println("🔴 LED OFF, Buzzer OFF, Servo back to 0° (vertical, closed)");
}

// Deny access - LED blinks, buzzer beeps 3 times, servo stays at 0°
void denyAccess() {
  Serial.println("🔴 Access Denied - LED blinking, Buzzer beeping 3 times, Servo stays at 0°");
  for (int i = 0; i < 3; i++) { // 3 beeps (match LED blinks)
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH); // Short beep
    delay(100);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}