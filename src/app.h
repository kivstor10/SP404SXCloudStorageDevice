#ifndef APP_H
#define APP_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

extern WiFiClientSecure net;
extern PubSubClient client;

// ----------- NETWORK ---------
void connectToWiFi();
void connectAWS();

// ----------- MQTT ------------
void mqttLoop();
void messageHandler(char* topic, byte* payload, unsigned int length);

// ----------- REGISTRATION ----
String generateRegistrationCode();
void checkRegistrationStatus(const String& deviceId);
void publishRegistrationCode(const String& deviceId, const String& code);

// ----------- SD CARD ---------
void setupSD();
void publishSDStatus();
uint64_t getAvailableSpace();

// ----------- TOPICS ----------
#define REG_CHECK_TOPIC_SUB "esp32/registration/status"
#define AWS_IOT_PUBLISH_TOPIC   "esp32/sd_status"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/commands"


// ----------- SHARED FLAGS ----
extern bool isDeviceRegistered;
extern bool receivedRegStatus;

#endif
