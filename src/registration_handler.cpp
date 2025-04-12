#include "app.h"
#include <ArduinoJson.h>

void checkRegistrationStatus(const String& deviceId) {
  Serial.println("Checking registration status...");
  // Logic for checking registration status to be added
}

String generateRegistrationCode() {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String code = "";
  for (int i = 0; i < 6; i++) {
    int index = random(0, sizeof(charset) - 1);
    code += charset[index];
  }
  return code;
}

void publishRegistrationCode(const String& deviceId, const String& regCode) {
  StaticJsonDocument<128> doc;
  doc["deviceId"] = deviceId;
  doc["registrationCode"] = regCode;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  String topic = "device/" + deviceId + "/registration-code";
  if (client.publish(topic.c_str(), jsonBuffer)) {
    Serial.println("Registration code published:");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("Failed to publish registration code");
  }
}
