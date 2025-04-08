#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <WiFiClientSecure.h>
#include <AWS_IOT.h>
 
#define SD_CS 5

 
// Write to the SD card 
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}
// Append data to the SD card 
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}


void setup() {
  Serial.begin(115200);
  // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("\n\nNo SD card attached");
    return;
  }
  Serial.println("\n\nInitializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialisation failed!");
    return;    // init failed
  }
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  Serial.printf("Available space: %lluMB\n", SD.totalBytes() / (1024 * 1024) - SD.usedBytes() / (1024 * 1024));
  // File file = SD.open("/data.txt");
  // if(!file) {
  //   writeFile(SD, "/data.txt", "Card initialised! \r\n");
  // }
  // else {
  //   Serial.println("File already exists");  
  // }
  // file.close();
}
void loop() {
  Serial.println("...");
  delay(1000);
}
 

