#include "app.h"
#include "secrets.h"
#define OLED_ADDR 0x3C // OLED display TWI address


bool isDeviceRegistered = false; 
bool receivedRegStatus = false;  

Adafruit_SSD1306 display(-1);

void setup() {
  Serial.begin(115200);
  delay(100);  // Debounce

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.invertDisplay(true);
  display.clearDisplay();
  delay(1000);
  display.display();
  display.invertDisplay(false);
  delay(10);
  display.setTextColor(SSD1306_WHITE);  
  display.setTextSize(1); 
  display.setCursor(28, 10); 
  display.print("Starting up...");
  display.display();

  // Setup SD card
  setupSD();


  // Connect to Wi-Fi and MQTT
  connectToWiFi();
  connectAWS();

  delay(2000);  // Give MQTT time to connect

  // Check registration
  randomSeed(analogRead(32)); // Seed random number generator using unused ADC pin
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
    display.clearDisplay();
    display.setTextSize(2); 
    display.setCursor(28, 10); 
    display.print(regCode);
    display.display();
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
