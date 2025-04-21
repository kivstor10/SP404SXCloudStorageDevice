#include "app.h"
#include <ArduinoJson.h>

String getDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();  // 48-bit MAC address
  char id[13];
  sprintf(id, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(id);
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
  StaticJsonDocument<256> doc; 
  doc["deviceId"] = deviceId;
  doc["registrationCode"] = regCode;

  size_t bufferSize = measureJson(doc) + 1; // +1 for null terminator
  char *jsonBuffer = new char[bufferSize];
  if (jsonBuffer == nullptr) {
    Serial.println("Failed to allocate memory for JSON buffer");
    return; // Handle the error!
  }
  serializeJson(doc, jsonBuffer, bufferSize);

  String topic = "devices/registration";
  if (client.publish(topic.c_str(), jsonBuffer)) {
    Serial.println("Registration code published:");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("Failed to publish registration code");
  }

  delete[] jsonBuffer; // Free the allocated memory
}



bool checkDeviceLinked(const String& deviceId) {
  String checkLinkEndpoint = "https://kdrqj7rc40.execute-api.eu-west-2.amazonaws.com/dev/check-device-link?deviceId=" + deviceId;
  bool linked = false;

  WiFiClientSecure httpsNet;
  httpsNet.setCACert(AWS_CERT_CA);       // Set the Root CA certificate (for server verification)
  httpsNet.setCertificate(AWS_CERT_CRT); // Set the device certificate (for client authentication, if required by the server)
  httpsNet.setPrivateKey(AWS_CERT_PRIVATE);  // Set the private key
  https.begin(httpsNet, checkLinkEndpoint.c_str());
  // https.addHeader("x-api-key", "YOUR_API_KEY");

  int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    linked = doc["isLinked"].as<bool>();
  } else {
    Serial.printf("Error checking device link: %d\\n", httpCode);
  }
  https.end();
  return linked;
}


