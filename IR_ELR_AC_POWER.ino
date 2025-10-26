#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

// IR LED pin configuration
const uint16_t kIrLedPin = 26;

// Initialize IR sender
IRsend irsend(kIrLedPin);

// Your captured KELON168 power toggle signal (21 bytes)
// This is the exact signal captured from your Hisense AC remote
const uint8_t powerToggleState[21] = {
  0x83, 0x06, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 
  0x00, 0x00, 0x00, 0x00, 0x01
};

// Function to send KELON168 protocol signal
void sendKELON168Signal(const uint8_t state[], uint16_t nbytes = 21) {
  Serial.println("\n--- TRANSMITTING KELON168 SIGNAL ---");
  
  // Send using KELON168 protocol
  // The library handles the proper three-section transmission format
  irsend.sendKelon168(state, nbytes);
  
  Serial.println("✓ Signal transmitted!");
  Serial.println("--- END TRANSMISSION ---\n");
}

// Function to toggle AC power (ON/OFF)
void toggleACPower() {
  Serial.println("\n=== TOGGLING AC POWER ===");
  Serial.println("Protocol: KELON168");
  Serial.println("Code: 0x830604010000000000000000000500010000000001");
  
  sendKELON168Signal(powerToggleState, 21);
  
  Serial.println("Power toggle command sent!");
  Serial.println("Note: This toggles between ON and OFF states");
  Serial.println("========================\n");
}

// Function to send signal multiple times (for reliability)
void sendMultipleTimes(int count = 3, int delayMs = 100) {
  Serial.printf("Sending power toggle %d times for reliability...\n", count);
  for (int i = 0; i < count; i++) {
    sendKELON168Signal(powerToggleState, 21);
    if (i < count - 1) {
      delay(delayMs);
    }
  }
  Serial.println("All transmissions complete!\n");
}

// Handle serial commands
void handleSerialCommands() {
  if (Serial.available()) {
    char command = Serial.read();
    
    // Clear any remaining newline characters
    while (Serial.available() && (Serial.peek() == '\n' || Serial.peek() == '\r')) {
      Serial.read();
    }
    
    switch (command) {
      case 'p':
      case 'P':
        toggleACPower();
        break;
        
      case 't':
      case 'T':
        sendMultipleTimes(3, 100);
        break;
        
      case 'r':
      case 'R':
        sendMultipleTimes(5, 150);
        break;
        
      case 'h':
      case 'H':
      case '?':
        printHelp();
        break;
        
      case '\n':
      case '\r':
        // Ignore newlines
        break;
        
      default:
        Serial.println("\nUnknown command. Type 'h' for help.");
        break;
    }
  }
}

// Print help menu
void printHelp() {
  Serial.println("\n========================================");
  Serial.println("   HISENSE AC CONTROL - COMMAND MENU");
  Serial.println("========================================");
  Serial.println("Commands:");
  Serial.println("  'p' - Toggle AC Power (ON/OFF)");
  Serial.println("  't' - Send power toggle 3 times (reliable)");
  Serial.println("  'r' - Send power toggle 5 times (max reliability)");
  Serial.println("  'h' - Show this help menu");
  Serial.println("========================================\n");
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Wait for serial monitor to open
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  ESP32 HISENSE AC IR CONTROL (KELON168)║");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.printf("║  IR LED Pin: GPIO %d                    ║\n", kIrLedPin);
  Serial.println("║  Protocol: KELON168                    ║");
  Serial.println("║  Baud Rate: 115200                     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Initialize IR transmitter
  irsend.begin();
  
  Serial.println("✓ IR Transmitter initialized successfully!");
  Serial.println();
  
  // Show help
  printHelp();
  
  Serial.println("Ready! Send commands via Serial Monitor.\n");
}

void loop() {
  // Handle serial commands
  handleSerialCommands();
  
  // Small delay to prevent overwhelming the serial buffer
  delay(10);
}