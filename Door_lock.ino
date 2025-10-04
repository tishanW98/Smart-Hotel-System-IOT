#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "POCOM3";      // Replace with your WiFi name
const char* password = "tishan1203";  // Replace with your WiFi password

// MQTT Broker settings (local Mosquitto)
const char* mqtt_server = "10.190.120.135";  // Replace with your computer's IP
const int mqtt_port = 1883;
// Remove username/password for local testing
const char* mqtt_topic_unlock = "door/unlock";
const char* mqtt_topic_status = "door/status";
const char* mqtt_topic_light = "light/control";  // New topic for LED strip

// Relay setup for LED strips
const int relayPin = 5;  // D27 (GPIO27) for relay (active-low)

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

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Authorized UID (replace with your card's UID)
byte authorizedUID[] = {0xEF, 0x27, 0x1E, 0x1E};

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✓ WiFi connected! IP: " + WiFi.localIP().toString());

  // Connect to MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  reconnectMQTT();
  client.subscribe(mqtt_topic_unlock);
  client.subscribe(mqtt_topic_light);  // Subscribe to light control

  // Initialize Relay for LED strips
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Relay OFF (active-low)
  Serial.println("✓ Relay initialized (LED strips OFF)");

  // Initialize SPI and RFID
  SPI.begin(19, 21, 23, SS_PIN); // SPI: SCK=D19, MISO=D21, MOSI=D23, SS=D18
  mfrc522.PCD_Init();
  Serial.println("===================");
  Serial.println("ESP32 + RFID + Web App + Servo + LED + Buzzer + LED Strips");
  Serial.println("===================");

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println("✓ LED initialized (OFF)");

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✓ Buzzer initialized (OFF)");

  // Initialize Servo
  myServo.attach(servoPin, minPulseWidth, maxPulseWidth);
  myServo.setPeriodHertz(50);
  myServo.writeMicroseconds(map(0, 0, 180, minPulseWidth, maxPulseWidth)); // Start at 0° (closed)
  Serial.println("✓ Servo initialized at 0 degrees (closed)");

  Serial.println("System Ready. Place RFID card or use web app to unlock or control lights...");
  publishStatus("locked");

  // Startup indicator
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print("Card UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();

    if (isAuthorized(mfrc522.uid.uidByte, mfrc522.uid.size)) {
      Serial.println("✅ RFID Authorized! ACCESS GRANTED");
      grantAccess();
    } else {
      Serial.println("❌ RFID Unauthorized! ACCESS DENIED");
      denyAccess();
    }
    mfrc522.PICC_HaltA();
    delay(1000);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("MQTT Message received on topic: " + String(topic) + " | Message: " + message);

  if (String(topic) == mqtt_topic_unlock && message == "unlock") {
    Serial.println("✅ Web App Unlock! ACCESS GRANTED");
    grantAccess();
  } else if (String(topic) == mqtt_topic_light) {
    if (message == "on") {
      digitalWrite(relayPin, LOW);  // Relay ON (active-low)
      Serial.println("LED Strips ON");
    } else if (message == "off") {
      digitalWrite(relayPin, HIGH); // Relay OFF
      Serial.println("LED Strips OFF");
    }
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32DoorLock")) {
      Serial.println("✓ Connected to MQTT!");
      client.subscribe(mqtt_topic_unlock);
      client.subscribe(mqtt_topic_light);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 5 seconds");
      delay(5000);
    }
  }
}

void publishStatus(const char* status) {
  client.publish(mqtt_topic_status, status);
}

bool isAuthorized(byte *uid, byte uidSize) {
  if (uidSize != sizeof(authorizedUID)) return false;
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}

void grantAccess() {
  Serial.println("🟢 LED ON, Buzzer long beep, Servo to 90° (open)");
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  int pulseWidth = map(90, 0, 180, minPulseWidth, maxPulseWidth);
  myServo.writeMicroseconds(pulseWidth);
  publishStatus("unlocked");

  delay(5000);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  pulseWidth = map(0, 0, 180, minPulseWidth, maxPulseWidth);
  myServo.writeMicroseconds(pulseWidth);
  publishStatus("locked");
  Serial.println("🔴 LED OFF, Buzzer OFF, Servo back to 0° (closed)");
}

void denyAccess() {
  Serial.println("🔴 Access Denied - LED blinking, Buzzer beeping 3 times, Servo stays at 0°");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}