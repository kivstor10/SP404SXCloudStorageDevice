#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>

#define AWS_IOT_PUBLISH_TOPIC   "esp32/sd_status"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/commands"
#define SD_CS 5  // SD card chip select pin

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

void messageHandler(char* topic, byte* payload, unsigned int length);

void setupSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.println("SD Card Initialized");
}

uint64_t getAvailableSpace() {
  if (SD.cardType() == CARD_NONE) return 0;
  return (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024); // Return in MB
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to MQTT broker
  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}
 
void publishSDStatus() {
  uint64_t freeSpaceMB = getAvailableSpace();
  uint64_t totalSpaceMB = SD.totalBytes() / (1024 * 1024);
  
  StaticJsonDocument<200> doc;
  doc["storage"]["free"] = freeSpaceMB;
  doc["storage"]["total"] = totalSpaceMB;
  doc["storage"]["unit"] = "MB";
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
 
  if (client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) {
    Serial.print("Published: ");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("Publish failed");
  }
}
 
void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  
  // Handle incoming commands if needed
  if (doc.containsKey("command")) {
    const char* command = doc["command"];
    Serial.print("Command received: ");
    Serial.println(command);
  }
}
 
void setup() {
  Serial.begin(115200);
  setupSD();
  connectAWS();

  delay(2000); // Allow time for WiFi to connect

  if (SD.cardType() != CARD_NONE) {
    publishSDStatus();
  } else {
    Serial.println("No SD card detected");
  }
}
 
void loop() {

}