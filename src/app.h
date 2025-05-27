#ifndef APP_H
#define APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h> 

#include <Arduino.h>

#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "secrets.h"


// extern WiFiClientSecure net;
// extern PubSubClient client;
extern HTTPClient https; 

// ----------- OLED DISPLAY ----
extern Adafruit_SSD1306 display;


// ----------- NETWORK ---------
extern WiFiClientSecure net;
extern PubSubClient client;

void connectToWiFi();
void connectAWS();

// ----------- MQTT ------------
void mqttLoop();
void messageHandler(char* topic, byte* payload, unsigned int length);

// ----------- REGISTRATION ----
String getDeviceId();
String generateRegistrationCode();
void publishRegistrationCode(const String& deviceId, const String& code);
bool checkDeviceLinked(const String& deviceId); // Declare the new function
void checkRegistrationStatus(const String& deviceId); // Existing, but might be adjusted

// ----------- SD CARD ---------
void setupSD();
void publishSDStatus();
uint64_t getAvailableSpace();

// ----------- OLED ------------
void showDeviceLinked();
void showSDRemoved();
void showReadyToUpload();
void showFileDownloadProgress(int currentFile, int totalFiles, int percent);
void showLinkCode(const String& regCode);
void waitForDeviceLink(const String& regCode);

// ----------- TOPICS ----------
#define REG_CHECK_TOPIC_SUB "esp32/registration/status"
#define AWS_IOT_PUBLISH_TOPIC "esp32/sd_status"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/commands"
// ----------- SHARED FLAGS ----
extern bool isDeviceRegistered;
extern bool receivedRegStatus;

// File download handler API 
void initFileDownloadHandler();
void enqueueDownloadUrl(const char* url, const char* s3Key);

#endif