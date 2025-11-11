// ESP32 MQTT AC Control - FIXED VERSION
// Uses correct sendKelon168() instead of sendRaw()

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// WiFi Credentials
const char* ssid = "POCOM3";
const char* password = "tishan1203";

// HiveMQ Cloud Settings
const char* mqtt_server = "32b18ba4678b4f3db60a560db8a90c86.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "wildwave";
const char* mqtt_password = "Wild@123";

// THIS ESP32 IS BOUND TO THIS AC ITEM CODE ONLY
const char* MY_ITEM_CODE = "1";

// IR Configuration
const uint16_t IR_LED_PIN = 26;
IRsend irsend(IR_LED_PIN);

// MQTT Topics
char mqtt_topic[50];
char current_ac_id[30] = "";
char current_item_code[30] = "";

// AC State
struct ACState {
  bool status = false;
  int temperature = 23;
  String mode = "Cool";
  String fanSpeed = "Low";
  bool access = true;
};

ACState currentState;

WiFiClientSecure espClient;
PubSubClient client(espClient);

// ===== IR CODE GENERATION FUNCTIONS =====

// Temperature to hex (Cooling)
uint8_t tempToHexCooling(int temp) {
  if (temp < 18 || temp > 30) return 0x72; // Default 25°C
  return 0x02 + ((temp - 18) * 0x10);
}

// Temperature to hex (Heating)
uint8_t tempToHexHeating(int temp) {
  if (temp < 18 || temp > 30) return 0x70; // Default 25°C
  return (temp - 18) * 0x10;
}

// Calculate checksum
uint8_t calculateChecksum(uint8_t tempHex, String fan, String mode) {
  if (mode == "Cool") {
    if (fan == "Auto") return tempHex;
    if (fan == "High") return tempHex + 1;
    if (fan == "Medium") return tempHex - 2;
    if (fan == "Low") return tempHex - 1;
  } else if (mode == "Heat") {
    if (fan == "Auto") return tempHex;
    if (fan == "Medium") return tempHex + 2;
  }
  return tempHex;
}

// ===== SEND IR COMMANDS =====

void sendPowerOff() {
  uint8_t code[21] = {
    0x83, 0x06, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01
  };
  
  Serial.println("→ Sending: Power OFF");
  Serial.print("   Code: 0x");
  for (int i = 0; i < 21; i++) {
    Serial.printf("%02X", code[i]);
  }
  Serial.println();
  
  // ✅ FIXED: Use sendKelon168() not sendRaw()
  irsend.sendKelon168(code, 21);
  delay(100);
}

void sendModeChange(String mode) {
  uint8_t code[21] = {
    0x83, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x06
  };
  
  if (mode == "Auto") {
    code[3] = 0x01;
    code[13] = 0x01;
  } else if (mode == "Dry") {
    code[3] = 0x03;
    code[13] = 0x03;
  } else if (mode == "Fan") {
    code[3] = 0x04;
    code[13] = 0x04;
  } else if (mode == "Cool") {
    code[3] = 0x62; // 24°C default
    code[13] = 0x62;
  }
  
  Serial.print("→ Sending: Mode = ");
  Serial.println(mode);
  
  // ✅ FIXED: Use sendKelon168() not sendRaw()
  irsend.sendKelon168(code, 21);
  delay(100);
}

void sendTempAndFan(int temp, String fan, String mode) {
  uint8_t code[21] = {
    0x83, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x11
  };
  
  // Fan speed
  if (fan == "Auto") code[2] = 0x00;
  else if (fan == "High") code[2] = 0x01;
  else if (fan == "Medium") code[2] = 0x02;
  else if (fan == "Low") code[2] = 0x03;
  
  // Temperature
  if (mode == "Cool") {
    code[3] = tempToHexCooling(temp);
  } else if (mode == "Heat") {
    code[3] = tempToHexHeating(temp);
  } else {
    code[3] = tempToHexCooling(temp);
  }
  
  // Checksum
  code[13] = calculateChecksum(code[3], fan, mode);
  
  Serial.print("→ Sending: ");
  Serial.print(temp);
  Serial.print("°C | Fan: ");
  Serial.print(fan);
  Serial.print(" | Mode: ");
  Serial.println(mode);
  Serial.print("   Code: 0x");
  for (int i = 0; i < 21; i++) {
    Serial.printf("%02X", code[i]);
  }
  Serial.println();
  
  // ✅ FIXED: Use sendKelon168() not sendRaw()
  irsend.sendKelon168(code, 21);
  delay(100);
}

// ===== APPLY AC CONTROL =====

void applyACControl() {
  Serial.println("\n=== APPLYING AC CONTROL ===");
  Serial.print("Status: ");
  Serial.println(currentState.status ? "ON" : "OFF");
  Serial.print("Temperature: ");
  Serial.print(currentState.temperature);
  Serial.println("°C");
  Serial.print("Mode: ");
  Serial.println(currentState.mode);
  Serial.print("Fan Speed: ");
  Serial.println(currentState.fanSpeed);
  Serial.print("Access: ");
  Serial.println(currentState.access ? "GRANTED" : "DENIED");
  
  // Check access
  if (!currentState.access) {
    Serial.println("⚠ Access DENIED! Forcing AC OFF");
    sendPowerOff();
    currentState.status = false;
    Serial.println("=========================\n");
    return;
  }
  
  // If OFF, send power off
  if (!currentState.status) {
    sendPowerOff();
    Serial.println("=========================\n");
    return;
  }
  
  // AC is ON - send commands
  Serial.println("Sending AC commands...");
  
  // Step 1: Set mode if needed
  if (currentState.mode == "Auto" || currentState.mode == "Dry" || currentState.mode == "Fan") {
    sendModeChange(currentState.mode);
  }
  
  // Step 2: Set temperature and fan
  if (currentState.mode == "Cool" || currentState.mode == "Heat") {
    sendTempAndFan(currentState.temperature, currentState.fanSpeed, currentState.mode);
  } else if (currentState.mode == "Auto") {
    // Auto mode with fan speed
    uint8_t code[21] = {
      0x83, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11,
      0x00, 0x00, 0x00, 0x00, 0x11
    };
    
    if (currentState.fanSpeed == "High") {
      code[2] = 0x01;
      code[13] = 0x00;
    } else if (currentState.fanSpeed == "Low") {
      code[2] = 0x03;
      code[13] = 0x02;
    }
    
    // ✅ FIXED: Use sendKelon168()
    irsend.sendKelon168(code, 21);
  }
  
  Serial.println("✓ AC commands sent successfully!");
  Serial.println("=========================\n");
}

// ===== WIFI & MQTT =====

void setup_wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Bound to AC itemCode: ");
  Serial.println(MY_ITEM_CODE);
}

void updateSubscription(const char* acId) {
  if (strlen(current_ac_id) > 0) {
    client.unsubscribe(mqtt_topic);
  }
  
  strncpy(current_ac_id, acId, sizeof(current_ac_id) - 1);
  current_ac_id[sizeof(current_ac_id) - 1] = '\0';
  
  snprintf(mqtt_topic, sizeof(mqtt_topic), "wildwaves/ac/%s", current_ac_id);
  
  client.subscribe(mqtt_topic);
  Serial.print("Subscribed to: ");
  Serial.println(mqtt_topic);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n=== Message on topic: ");
  Serial.print(topic);
  Serial.println(" ===");
  
  StaticJsonDocument<400> doc;
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  
  Serial.print("JSON: ");
  Serial.println(messageTemp);
  
  DeserializationError error = deserializeJson(doc, messageTemp);
  
  if (error) {
    Serial.print("Parse Error: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Check itemCode
  const char* itemCode = doc["itemCode"];
  
  if (itemCode) {
    if (strcmp(itemCode, MY_ITEM_CODE) != 0) {
      Serial.print("Ignoring - itemCode '");
      Serial.print(itemCode);
      Serial.print("' != '");
      Serial.print(MY_ITEM_CODE);
      Serial.println("'");
      return;
    }
    
    Serial.print("✓ itemCode matches: ");
    Serial.println(MY_ITEM_CODE);
  } else {
    Serial.println("No itemCode, ignoring...");
    return;
  }
  
  // Subscribe to acId
  const char* acId = doc["acId"];
  if (acId && (strlen(current_ac_id) == 0 || strcmp(current_ac_id, acId) != 0)) {
    updateSubscription(acId);
  }
  
  // Check event
  const char* event = doc["event"];
  
  if (event && strcmp(event, "ac_control") == 0) {
    
    bool needsUpdate = false;
    
    // Status
    // Old code
    // if (doc.containsKey("status")) {
    //   bool newStatus = doc["status"].as<bool>();
    //   if (newStatus != currentState.status) {
    //     currentState.status = newStatus;
    //     needsUpdate = true;
    //   }
    // }
    // ✅ NEW CODE (always updates):
    if (doc.containsKey("status")) {
      bool newStatus = doc["status"].as<bool>();
      currentState.status = newStatus;  // ← NO IF CHECK
      needsUpdate = true;
      Serial.print("Status updated: ");
      Serial.println(newStatus ? "ON" : "OFF");
    }
    
    // Temperature
    // Old code
    // if (doc.containsKey("temperaturelevel")) {
    //   int newTemp = doc["temperaturelevel"].as<int>();
    //   newTemp = constrain(newTemp, 18, 30);
    //   if (newTemp != currentState.temperature) {
    //     currentState.temperature = newTemp;
    //     needsUpdate = true;
    //   }
    // }
    // ✅ NEW CODE (checks BOTH names):
    if (doc.containsKey("temperaturelevel")) {
      int newTemp = doc["temperaturelevel"].as<int>();
      newTemp = constrain(newTemp, 18, 30);
      currentState.temperature = newTemp;
      needsUpdate = true;
      Serial.print("Temperature updated: ");
      Serial.println(newTemp);
    } else if (doc.containsKey("temperature")) {  // ← ADDED THIS
      int newTemp = doc["temperature"].as<int>();
      newTemp = constrain(newTemp, 18, 30);
      currentState.temperature = newTemp;
      needsUpdate = true;
      Serial.print("Temperature updated: ");
      Serial.println(newTemp);
    }
    
    // Mode
    if (doc.containsKey("mode")) {
      String newMode = doc["mode"].as<String>();
      if (newMode != currentState.mode) {
        currentState.mode = newMode;
        needsUpdate = true;
      }
    }
    
    // Fan Speed
    if (doc.containsKey("fanSpeed")) {
      String newFanSpeed = doc["fanSpeed"].as<String>();
      if (newFanSpeed != currentState.fanSpeed) {
        currentState.fanSpeed = newFanSpeed;
        needsUpdate = true;
      }
    }
    
    // Access
    if (doc.containsKey("access")) {
      bool newAccess = doc["access"].as<bool>();
      if (newAccess != currentState.access) {
        currentState.access = newAccess;
        needsUpdate = true;
      }
    }
    
    // Apply if changed
    if (needsUpdate) {
      applyACControl();
    } else {
      Serial.println("No fields to update in message");
    }
    
  } else {
    Serial.println("Unknown event");
  }
  
  Serial.println("=== End ===\n");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT... ");
    String clientId = "ESP32-AC-" + String(MY_ITEM_CODE) + "-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected!");
      
      client.subscribe("wildwaves/ac/+");
      Serial.println("Subscribed to: wildwaves/ac/+");
      Serial.print("Listening for: ");
      Serial.println(MY_ITEM_CODE);
      
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

// ===== SETUP =====

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // ✅ CRITICAL: Initialize IR sender
  irsend.begin();
  
  Serial.println("\n========================================");
  Serial.println("ESP32 MQTT AC Controller - FIXED");
  Serial.println("========================================");
  Serial.print("ItemCode: ");
  Serial.println(MY_ITEM_CODE);
  Serial.print("IR LED: GPIO ");
  Serial.println(IR_LED_PIN);
  Serial.println("Using: sendKelon168() ✓");
  Serial.println("========================================\n");
  
  setup_wifi();
  
  espClient.setInsecure();
  espClient.setTimeout(15000);
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setSocketTimeout(15);
  
  Serial.println("Setup complete! Ready for MQTT...\n");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}