// ESP32 MQTT Light Control with MOSFET (PWM Dimming)
// Replaces relay with IRLB8721 MOSFET for brightness control

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/// WiFi Credentials
const char* ssid = "POCOM3";
const char* password = "tishan1203";

// HiveMQ Cloud Settings
const char* mqtt_server = "32b18ba4678b4f3db60a560db8a90c86.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "wildwave";
const char* mqtt_password = "Wild@123";

// THIS ESP32 IS BOUND TO THIS ITEM CODE ONLY
const char* MY_ITEM_CODE = "L-0001";  // Change this for different ESP32 devices

// MQTT Topics
char mqtt_topic[50];
char current_light_id[30] = "";
char current_item_code[30] = "";

// MOSFET PWM Configuration
const int MOSFET_PIN = 23;        // GPIO pin for MOSFET gate
const int LEDC_CHANNEL = 0;
const int LEDC_FREQ = 5000;       // 5kHz PWM frequency
const int LEDC_RESOLUTION = 10;   // 10-bit resolution (0-1023)
const int MAX_DUTY = 1023;

int currentBrightness = 0;        // Store current brightness (0-100)
bool currentStatus = false;       // Store current status (true=ON, false=OFF)

WiFiClientSecure espClient;
PubSubClient client(espClient);

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
  Serial.print("This ESP32 is bound to itemCode: ");
  Serial.println(MY_ITEM_CODE);
}

void applyLight(int brightness, bool status) {
  currentBrightness = brightness;
  currentStatus = status;
  
  int actualBrightness = brightness;
  
  // If status is OFF, force brightness to 0 (light off)
  if (!status) {
    actualBrightness = 0;
  }
  
  // Constrain brightness to 0-100 range
  actualBrightness = constrain(actualBrightness, 0, 100);
  
  // Map brightness (0-100) to PWM duty cycle (0-1023)
  int duty = map(actualBrightness, 0, 100, 0, MAX_DUTY);
  ledcWrite(LEDC_CHANNEL, duty);
  
  Serial.print("Status: ");
  Serial.print(status ? "ON" : "OFF");
  Serial.print(" | Brightness Setting: ");
  Serial.print(brightness);
  Serial.print("% | Actual Output: ");
  Serial.print(actualBrightness);
  Serial.print("% (PWM: ");
  Serial.print(duty);
  Serial.println(")");
}

void updateSubscription(const char* lightId) {
  // Unsubscribe from old topic if necessary
  if (strlen(current_light_id) > 0) {
    client.unsubscribe(mqtt_topic);
  }
  
  // Update the current lightId
  strncpy(current_light_id, lightId, sizeof(current_light_id) - 1);
  current_light_id[sizeof(current_light_id) - 1] = '\0';
  
  // Create new topic string
  snprintf(mqtt_topic, sizeof(mqtt_topic), "wildwaves/lights/%s", current_light_id);
  
  // Subscribe to new topic
  client.subscribe(mqtt_topic);
  Serial.print("Subscribed to topic: ");
  Serial.println(mqtt_topic);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n=== Message received on topic: ");
  Serial.print(topic);
  Serial.println(" ===");
  
  StaticJsonDocument<300> doc;
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  
  Serial.print("Raw JSON: ");
  Serial.println(messageTemp);
  
  DeserializationError error = deserializeJson(doc, messageTemp);
  
  if (error) {
    Serial.print("JSON Parse Error: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Extract itemCode first - THIS IS CRITICAL
  const char* itemCode = doc["itemCode"];
  
  // CHECK IF THIS MESSAGE IS FOR THIS ESP32
  if (itemCode) {
    if (strcmp(itemCode, MY_ITEM_CODE) != 0) {
      Serial.print("Ignoring message - itemCode '");
      Serial.print(itemCode);
      Serial.print("' does not match MY_ITEM_CODE '");
      Serial.print(MY_ITEM_CODE);
      Serial.println("'");
      return;  // EXIT - This message is NOT for this ESP32
    }
    
    // Store the itemCode
    strncpy(current_item_code, itemCode, sizeof(current_item_code) - 1);
    current_item_code[sizeof(current_item_code) - 1] = '\0';
    
    Serial.print("✓ itemCode matches! Processing message for: ");
    Serial.println(MY_ITEM_CODE);
  } else {
    Serial.println("Warning: No itemCode in message, ignoring...");
    return;
  }
  
  // Extract the lightId and subscribe if needed
  const char* lightId = doc["lightId"];
  if (lightId && (strlen(current_light_id) == 0 || strcmp(current_light_id, lightId) != 0)) {
    updateSubscription(lightId);
  }
  
  // Extract event
  const char* event = doc["event"];
  
  if (event && strcmp(event, "light_control") == 0) {
    
    // Get status - handle both boolean and string formats
    bool hasStatus = doc.containsKey("status");
    bool status = currentStatus;  // Default to current status
    
    if (hasStatus) {
      if (doc["status"].is<bool>()) {
        status = doc["status"].as<bool>();
      } else if (doc["status"].is<const char*>()) {
        const char* statusStr = doc["status"];
        status = (strcmp(statusStr, "ON") == 0 || strcmp(statusStr, "true") == 0);
      }
    }
    
    // Get brightness - use current if not provided
    bool hasBrightness = doc.containsKey("brightness");
    int brightness = currentBrightness;
    
    if (hasBrightness) {
      brightness = doc["brightness"].as<int>();
      brightness = constrain(brightness, 0, 100);
    }
    
    // Get access control
    bool access = doc["access"] | true;
    
    Serial.print("Parsed - Status: ");
    Serial.print(status ? "ON" : "OFF");
    Serial.print(" | Brightness: ");
    Serial.print(brightness);
    Serial.print("% | Access: ");
    Serial.println(access ? "GRANTED" : "DENIED");
    
    // Check access control
    if (!access) {
      Serial.println("⚠ Access DENIED! Forcing light OFF");
      applyLight(0, false);
      return;
    }
    
    // Apply the control
    applyLight(brightness, status);
    
  } else {
    Serial.println("Unknown event or missing event field");
  }
  
  Serial.println("=== End of message processing ===\n");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT... ");
    String clientId = "ESP32-" + String(MY_ITEM_CODE) + "-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected!");
      
      // Subscribe to wildcard topic to receive messages for all lights
      client.subscribe("wildwaves/lights/+");
      Serial.println("Subscribed to: wildwaves/lights/+");
      Serial.print("Listening for itemCode: ");
      Serial.println(MY_ITEM_CODE);
      
    } else {
      int state = client.state();
      Serial.print("failed, rc=");
      Serial.print(state);
      Serial.print(" (");
      switch (state) {
        case -4: Serial.print("MQTT_CONNECTION_TIMEOUT"); break;
        case -3: Serial.print("MQTT_CONNECTION_LOST"); break;
        case -2: Serial.print("MQTT_CONNECT_FAILED"); break;
        case -1: Serial.print("MQTT_DISCONNECTED"); break;
        case 1: Serial.print("MQTT_CONNECT_BAD_PROTOCOL"); break;
        case 2: Serial.print("MQTT_CONNECT_BAD_CLIENT_ID"); break;
        case 3: Serial.print("MQTT_CONNECT_UNAVAILABLE"); break;
        case 4: Serial.print("MQTT_CONNECT_BAD_CREDENTIALS"); break;
        case 5: Serial.print("MQTT_CONNECT_UNAUTHORIZED"); break;
      }
      Serial.println(") retrying in 5s...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Setup PWM for MOSFET control
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION);
  ledcAttachPin(MOSFET_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, 0);  // Start with light OFF
  
  Serial.println("\n========================================");
  Serial.println("ESP32 MQTT Light Controller");
  Serial.println("========================================");
  Serial.print("Bound to itemCode: ");
  Serial.println(MY_ITEM_CODE);
  Serial.println("PWM initialized on GPIO 23");
  Serial.println("========================================\n");
  
  setup_wifi();
  
  // SSL/TLS Configuration
  espClient.setInsecure();
  espClient.setTimeout(15000);
  
  // MQTT Configuration
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setSocketTimeout(15);
  
  Serial.println("\nSetup complete! Waiting for MQTT messages...");
  Serial.print("Will only respond to itemCode: ");
  Serial.println(MY_ITEM_CODE);
  Serial.println("========================================\n");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}