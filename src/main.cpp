#include "app.h"
#include "esp_heap_caps.h"
#include <time.h>
#define OLED_ADDR 0x3C // OLED display TWI address


bool isDeviceRegistered = false;
bool receivedRegStatus = false;
bool sdInserted = false;
unsigned long lastSDCheck = 0;
const unsigned long sdCheckInterval = 2000; // ms

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
  delay(100);   // Debounce

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

  // Initialize file download handler (tasks, queues) FIRST
  initFileDownloadHandler();

  // Setup SD card
  setupSD();

  // Connect to Wi-Fi and MQTT
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(28, 7);
  display.print("Connecting to");
  display.setCursor(25, 17);
  display.print(WIFI_SSID);
  display.display();
  connectToWiFi();
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(28, 7);
  display.print("Connecting to");
  display.setCursor(10, 17);
  display.print("SP Cloud Servers...");
  display.display();
  connectAWS();

  delay(2000);   // Give MQTT time to connect

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
  
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
  Serial.print("Largest free block: ");
  Serial.println(largest_block);
  client.setCallback(messageHandler);

  // Set system time via NTP before any HTTPS requests
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 100000) {
      delay(100);
      now = time(nullptr);
  }
  Serial.println("done.");
}

void loop() {
  mqttLoop();

  static bool lastCardPresent = false;
  static unsigned long lastSDQuery = 0;
  static bool cardPresent = false;

  // Only check SD card every sdCheckInterval ms
  if (millis() - lastSDQuery > sdCheckInterval) {
    lastSDQuery = millis();
    File testFile = SD.open("/");
    if (testFile) {
      cardPresent = true;
      testFile.close();
    } else {
      cardPresent = false;
    }
  }

  // Detect SD card removal
  if (lastCardPresent && !cardPresent) {
    sdInserted = false;
    showSDRemoved();
    lastSDCheck = millis();
  }

  // Detect SD card insertion
  if (!lastCardPresent && cardPresent) {
    sdInserted = true;
    showReadyToUpload();
  }

  lastCardPresent = cardPresent;

  // Only try to initialize if not inserted and card is not present
  if (!sdInserted && !cardPresent) {
    if (millis() - lastSDCheck > sdCheckInterval) {
      lastSDCheck = millis();
      if (SD.begin()) {
        File testFile2 = SD.open("/");
        if (testFile2) {
          cardPresent = true;
          testFile2.close();
        } else {
          cardPresent = false;
        }
        if (cardPresent) {
          sdInserted = true;
          showReadyToUpload();
        } else {
          sdInserted = false;
          showSDRemoved();
        }
      } else {
        showSDRemoved();
      }
    } else {
      showSDRemoved();
    }
  }

  static unsigned long lastHeapLogTime = 0;
  if (millis() - lastHeapLogTime > 5000) { // Log heap every 5 seconds
    Serial.printf("[MainLoop] Free Heap: %u, Min Free Heap: %u\n",
                  (unsigned int)ESP.getFreeHeap(),
                  (unsigned int)ESP.getMinFreeHeap());
    lastHeapLogTime = millis();
  }
}
