#include "app.h"


// External shared flags 
extern bool isDeviceRegistered;
extern bool receivedRegStatus;


void messageHandler(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);

    // Allocate a buffer and copy payload into a null-terminated string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        return;
    }

    if (String(topic) == REG_CHECK_TOPIC_SUB) {
        if (doc.containsKey("registered")) {
            isDeviceRegistered = doc["registered"];
            receivedRegStatus = true;
            Serial.printf("Device registration status: %s\n",
                          isDeviceRegistered ? "Registered" : "Unregistered");
        } else {
            Serial.println("No 'registered' key in response");
        }
    } else {
        Serial.println("Topic does not match registration check");
    }
}
