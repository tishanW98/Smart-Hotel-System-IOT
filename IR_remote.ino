#include <Arduino.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <IRsend.h>

// Receiver pin (IR sensor)
const uint16_t kRecvPin = 22;

// Transmitter pin (IR LED)
const uint16_t kIrLedPin = 26;

// PWM settings for IR LED
const int pwmFreq = 1000;  // 1kHz PWM frequency (base for transistor drive)
const int pwmChannel = 0;  // PWM channel (0-15 on ESP32)
const int pwmResolution = 8;  // 8-bit resolution (0-255 duty cycle)

const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;

#if DECODE_AC
const uint8_t kTimeout = 50;
#else
const uint8_t kTimeout = 15;
#endif

const uint16_t kMinUnknownSize = 12;
const uint8_t kTolerancePercentage = kTolerance;

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
IRsend irsend(kIrLedPin);
decode_results results;

// Variables to store the last captured signal
uint16_t *lastRawData = nullptr;
uint16_t lastRawDataLen = 0;
uint32_t lastAddress = 0;
uint32_t lastCommand = 0;
uint64_t lastData = 0;
String lastProtocol = "";
decode_type_t lastDecodeType = decode_type_t::UNKNOWN;

void setup() {
  // Set up PWM channel for IR LED
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(kIrLedPin, pwmChannel);
  ledcWrite(pwmChannel, 0);  // Ensure LED is off initially

#if defined(ESP8266)
  Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
#else
  Serial.begin(kBaudRate, SERIAL_8N1);
#endif
  
  while (!Serial)
    delay(50);

  Serial.printf("\nIR Receiver started on pin %d\n", kRecvPin);
  Serial.printf("IR Transmitter ready on pin %d\n", kIrLedPin);
  Serial.println("\n=== KNOWN PROTOCOLS ONLY MODE ===");
  Serial.println("This will only capture and store recognized IR protocols.");
  Serial.println("UNKNOWN signals will be ignored.\n");
  Serial.println("Commands:");
  Serial.println("  't' - Transmit last captured signal at max brightness");
  Serial.println("  'r' - Repeat last signal 5 times at max brightness");
  Serial.println("  's' - Show last captured signal info");
  Serial.println();
  
#if DECODE_HASH
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif
  
  irrecv.setTolerance(kTolerancePercentage);
  irrecv.enableIRIn();
  
  // Initialize IR transmitter
  irsend.begin();
}

bool isKnownProtocol(decode_type_t protocol) {
  return (protocol != decode_type_t::UNKNOWN);
}

void storeSignal(decode_results *results) {
  if (!isKnownProtocol(results->decode_type)) {
    return;
  }
  
  if (lastRawData != nullptr) {
    delete[] lastRawData;
    lastRawData = nullptr;
  }
  
  if (results->rawlen > 0) {
    lastRawDataLen = results->rawlen;
    lastRawData = new uint16_t[lastRawDataLen];
    for (uint16_t i = 0; i < lastRawDataLen; i++) {
      lastRawData[i] = results->rawbuf[i] * kRawTick;
    }
  }
  
  lastDecodeType = results->decode_type;
  lastProtocol = typeToString(results->decode_type, results->repeat);
  lastAddress = results->address;
  lastCommand = results->command;
  lastData = results->value;
  
  Serial.println("✓ Known protocol signal captured and stored!");
}

void showLastSignalInfo() {
  if (lastRawData == nullptr || lastRawDataLen == 0) {
    Serial.println("No signal stored!");
    return;
  }
  
  Serial.println("\n=== STORED SIGNAL INFO ===");
  Serial.printf("Protocol: %s\n", lastProtocol.c_str());
  Serial.printf("Address: 0x%X\n", lastAddress);
  Serial.printf("Command: 0x%X\n", lastCommand);
  Serial.printf("Data: 0x%llX\n", lastData);
  Serial.printf("Raw data length: %d\n", lastRawDataLen);
  Serial.println("==========================\n");
}

void transmitLastSignal() {
  if (lastRawData == nullptr || lastRawDataLen == 0) {
    Serial.println("No signal stored to transmit!");
    return;
  }
  
  Serial.println("\n--- TRANSMITTING SIGNAL ---");
  Serial.printf("Protocol: %s\n", lastProtocol.c_str());
  Serial.printf("Data: 0x%llX\n", lastData);
  Serial.printf("Raw data length: %d\n", lastRawDataLen);
  
  // Set PWM to max (100% duty cycle) for maximum brightness
  ledcWrite(pwmChannel, 255);
  // Transmit the raw signal
  irsend.sendRaw(lastRawData, lastRawDataLen, 38); // 38kHz frequency
  // Turn off LED after transmission
  ledcWrite(pwmChannel, 0);
  
  Serial.println("✓ Signal transmitted at max brightness!");
  Serial.println("--- END TRANSMISSION ---\n");
}

void handleSerialCommands() {
  if (Serial.available()) {
    char command = Serial.read();
    
    switch (command) {
      case 't':
      case 'T':
        transmitLastSignal();
        break;
        
      case 'r':
      case 'R':
        Serial.println("Repeating last signal 5 times at max brightness...");
        for (int i = 0; i < 5; i++) {
          transmitLastSignal();
          delay(100);
        }
        break;
        
      case 's':
      case 'S':
        showLastSignalInfo();
        break;
        
      case '\n':
      case '\r':
        break;
        
      default:
        Serial.println("Unknown command.");
        Serial.println("Use: 't' = transmit, 'r' = repeat 5x, 's' = show info");
        break;
    }
  }
}

void loop() {
  handleSerialCommands();
  
  if (irrecv.decode(&results)) {
    if (isKnownProtocol(results.decode_type)) {
      uint32_t now = millis();
      Serial.printf("\n" D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
      
      if (results.overflow)
        Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
      
      storeSignal(&results);
      Serial.print(resultToHumanReadableBasic(&results));
      
      String description = IRAcUtils::resultAcToString(&results);
      if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
      
      yield();
      Serial.println(resultToSourceCode(&results));
      
      Serial.println("Send 't' to transmit or 'r' to repeat 5 times");
      Serial.println();
      
      yield();
    } else {
      Serial.println("[Ignored: UNKNOWN protocol]");
    }
    
    irrecv.resume();
  }
}