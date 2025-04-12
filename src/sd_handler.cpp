#include "app.h"

#define SD_CS 5  // SD card chip select pin

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
