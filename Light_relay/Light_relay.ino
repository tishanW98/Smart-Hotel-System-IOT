#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "POCOM3";
const char* password = "tishan1203";

// HiveMQ Cloud Settings
const char* mqtt_server = "32b18ba4678b4f3db60a560db8a90c86.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "wildwave";
const char* mqtt_password = "Wild@123";

// MQTT Topics
char mqtt_topic[50];  // Dynamic topic string to store the topic with lightId
char current_light_id[30] = "";  // To store the current lightId

// Relay Pin
const int RELAY_PIN = 26;  // Using pin 26 for relay control

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
  Serial.print("Subscribed to new topic: ");
  Serial.println(mqtt_topic);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  
  StaticJsonDocument<200> doc;
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  
  Serial.print("Raw message: ");
  Serial.println(messageTemp);
  
  DeserializationError error = deserializeJson(doc, messageTemp);
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Extract the lightId from the message
  const char* lightId = doc["lightId"];
  if (lightId && (strlen(current_light_id) == 0 || strcmp(current_light_id, lightId) != 0)) {
    updateSubscription(lightId);
  }
  
  const char* status = doc["status"];
  const char* event = doc["event"];
  
  if (event && strcmp(event, "light_control") == 0 && status) {
    Serial.print("Status received: ");
    Serial.println(status);
    
    if (strcmp(status, "ON") == 0) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Relay turned ON");
      Serial.print("Pin ");
      Serial.print(RELAY_PIN);
      Serial.println(" set to HIGH");
    } 
    else if (strcmp(status, "OFF") == 0) {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Relay turned OFF");
      Serial.print("Pin ");
      Serial.print(RELAY_PIN);
      Serial.println(" set to LOW");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT... ");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected!");
      
      // If we have a stored lightId, subscribe to its topic
      if (strlen(current_light_id) > 0) {
        snprintf(mqtt_topic, sizeof(mqtt_topic), "wildwaves/lights/%s", current_light_id);
        client.subscribe(mqtt_topic);
        Serial.print("Subscribed to topic: ");
        Serial.println(mqtt_topic);
      }
      
      // Also subscribe to a general topic to receive initial lightId
      client.subscribe("wildwaves/lights/+");
      Serial.println("Subscribed to wildcard topic: wildwaves/lights/+");
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
  
  // Set relay pin as output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  setup_wifi();
  
  // SSL/TLS Configuration
  espClient.setInsecure();
  espClient.setTimeout(15000);
  
  // MQTT Configuration
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setSocketTimeout(15);

  Serial.println("Setup complete, waiting for MQTT messages...");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
}