#include "app.h"
#define OLED_ADDR 0x3C // OLED display TWI address


bool isDeviceRegistered = false;
bool receivedRegStatus = false;

Adafruit_SSD1306 display(-1);
WiFiClientSecure net;
PubSubClient client(net);
HTTPClient https;

// Forward declarations (now in app.h, but good practice to have here too)
String getDeviceId();
String generateRegistrationCode();
void publishRegistrationCode(const String& deviceId, const String& code);
bool checkDeviceLinked(const String& deviceId);
void checkRegistrationStatus(const String& deviceId);

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
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(28, 7);
  display.print("Connecting to");
  display.setCursor(30, 10);
  display.print(WIFI_SSID);
  display.display();
  connectToWiFi();
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(28, 7);
  display.print("Connecting to");
  display.setCursor(30, 10);
  display.print("SP Cloud Servers...");
  display.display();
  connectAWS();

  delay(2000);  // Give MQTT time to connect

  // Get Device ID
  String deviceId = getDeviceId();

  // Check if device is linked
  if (checkDeviceLinked(deviceId)) {
    isDeviceRegistered = true;
    showDeviceLinked(); 
    Serial.println("Device already linked.");
  } else {
    isDeviceRegistered = false;
    Serial.println("Device not linked. Proceeding with registration.");


    // If still unregistered, publish code
    if (!isDeviceRegistered) {
      String regCode = generateRegistrationCode();
      publishRegistrationCode(deviceId, regCode);
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(28, 10);
      display.print(regCode);
      display.display();
    } else {
      Serial.println("Device registered (linked). Skipping code generation on OLED."); //Changed the message
    }
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