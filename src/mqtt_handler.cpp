#include "app.h"

// External shared flags
extern bool isDeviceRegistered;
extern bool receivedRegStatus;

// Function to trim whitespace from a string (C++ style)
String trimString(String str) {
  str.trim(); // Remove leading and trailing whitespace
  return str;
  }

void messageHandler(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, REG_CHECK_TOPIC_SUB) == 0) {
    // Convert the payload to a string
    String message = "";
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print("Received registration message: ");
    Serial.println(message);

    // Extract the device ID from the message.  The message is JSON.
    int deviceIdStartIndex = message.indexOf("\"deviceId\":\"");
    if (deviceIdStartIndex != -1) {
      deviceIdStartIndex += 12; // Length of ""deviceId":"
      int deviceIdEndIndex = message.indexOf("\"", deviceIdStartIndex);
      if (deviceIdEndIndex != -1) {
        String receivedDeviceId = message.substring(deviceIdStartIndex, deviceIdEndIndex);
        
        // Get the device's own ID using the function
        String myDeviceId = getDeviceId();

        // Trim both strings before comparison
        receivedDeviceId = trimString(receivedDeviceId);
        myDeviceId = trimString(myDeviceId);

        String status = "";
        int statusStartIndex = message.indexOf("\"status\":\"");
        if(statusStartIndex != -1){
           statusStartIndex += 10; //length of "status":"
           int statusEndIndex = message.indexOf("\"",statusStartIndex);
           if(statusEndIndex != -1){
             status = message.substring(statusStartIndex,statusEndIndex);
           }
        }
        

        if (status == "linked" && receivedDeviceId == myDeviceId) {
          isDeviceRegistered = true;
          Serial.println("Device is now linked!");
          showDeviceLinked(); // Call the function here
        } else if (status == "not linked" && receivedDeviceId == myDeviceId) {
          isDeviceRegistered = false;
          Serial.println("Device is not linked!");
        } else {
          Serial.print("Device ID does not match.  Received: ");
          Serial.print(receivedDeviceId);
          Serial.print("  Expected: ");
          Serial.println(myDeviceId);
        }
      } else {
         Serial.println("Error: Could not find end of deviceId in JSON");
      }
    } else {
      Serial.println("Error: Could not find deviceId in JSON");
    }
  }
}
