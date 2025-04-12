#include "app.h"
#include "secrets.h"

bool isDeviceRegistered = false; 
bool receivedRegStatus = false;  

void setup() {
  Serial.begin(115200);
  delay(100);  // Debounce

  // Setup SD card
  setupSD();

  // Connect to Wi-Fi and MQTT
  connectToWiFi();
  connectAWS();

  delay(2000);  // Give MQTT time to connect

  // Check registration
  randomSeed(analogRead(0));
  String deviceId = THINGNAME;
  checkRegistrationStatus(deviceId);

  // Wait max 5s for registration status
  unsigned long startTime = millis();
  while (!receivedRegStatus && millis() - startTime < 5000) {
    mqttLoop();  // Keep MQTT alive and process messages
    delay(100);
  }

  // If unregistered, publish code
  if (!isDeviceRegistered) {
    String regCode = generateRegistrationCode();
    publishRegistrationCode(deviceId, regCode);
  } else {
    Serial.println("Device already registered. Skipping code generation.");
  }

  // Publish SD info
  if (SD.cardType() != CARD_NONE) {
    publishSDStatus();
  } else {
    Serial.println("No SD card detected");
  }
}

void loop() {
  mqttLoop();  // Process MQTT messages
}
