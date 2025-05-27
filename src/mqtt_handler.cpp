#include "app.h" // Should declare: extern bool isDeviceRegistered; extern String getDeviceId(); extern void showDeviceLinked();
                 // And importantly: void enqueueDownloadUrl(const char* url, const char* s3Key);
                 // Also, REG_CHECK_TOPIC_SUB (as a #define or const char*)
#include <ArduinoJson.h>

// External shared flags - ensure these are properly defined/declared elsewhere
extern bool isDeviceRegistered;
// extern bool receivedRegStatus; // Not used in this specific handler logic

// Function to trim whitespace from a string (C++ style)
// String trimString(String str) { // This function is not used in this specific version of messageHandler
//   str.trim(); 
//   return str;
// }

void waitForDeviceLink() {
    while (!isDeviceRegistered) {
        showDeviceLinked(); // Or show a 'Please link device' message if you have one
        delay(500); // Poll every 500ms
        // Optionally, add a timeout or button to break out
    }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  // ---- New Detailed Logging - START ----
  Serial.println(); 
  Serial.println("--- MQTT Callback Invoked ---");
  
  char localTopicCopy[256]; // Buffer to safely copy and print the topic
  if (topic) {
    strncpy(localTopicCopy, topic, sizeof(localTopicCopy) - 1);
    localTopicCopy[sizeof(localTopicCopy) - 1] = '\0'; // Ensure null termination
    Serial.print("Callback received on raw topic: '");
    Serial.print(localTopicCopy);
    Serial.println("'");
    Serial.print("Raw topic length (strlen): "); Serial.println(strlen(localTopicCopy));
  } else {
    Serial.println("Callback received with NULL topic pointer!");
    localTopicCopy[0] = '\0'; // Empty string if topic is NULL
  }
  Serial.print("Payload length: "); Serial.println(length);
  // ---- New Detailed Logging - END ----

  // --- Presigned URL Topic Check ---
  String deviceId_from_callback = getDeviceId(); 
  String presignedUrlTopic_constructed = "/presignedurls/" + deviceId_from_callback;

  // ---- More Detailed Logging for Presigned URL Topic Comparison ----
  Serial.print("Device ID from getDeviceId() INSIDE CALLBACK: '"); Serial.print(deviceId_from_callback); Serial.println("'");
  Serial.print("Constructed presignedUrlTopic INSIDE CALLBACK: '"); Serial.print(presignedUrlTopic_constructed); Serial.println("'");
  // ---- End Detailed Logging ----

  if (strcmp(localTopicCopy, presignedUrlTopic_constructed.c_str()) == 0) {
    Serial.println(">>> PRESIGNED URL TOPIC MATCH INSIDE CALLBACK <<<");
    Serial.print("Presigned URL message [");
    Serial.print(localTopicCopy); 
    Serial.print("]: ");
    
    String urlPayload = "";
    for (unsigned int i = 0; i < length; i++) {
      urlPayload += (char)payload[i];
    }
    Serial.println(urlPayload);

    // Parse JSON and extract presignedUrl(s)
    // Adjust JSON document size based on expected max payload for a single batch from Lambda
    StaticJsonDocument<2048> doc; // Increased from 1500 to 2048 to handle larger payloads and avoid NoMemory error
    DeserializationError error = deserializeJson(doc, urlPayload);

    if (!error && doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      int totalFilesInBatch = arr.size();
      int fileIndex = 1;
      Serial.printf("[MQTT] Presigned URL batch contains %d file(s).\n", totalFilesInBatch);

      for (JsonObject obj : arr) {
        const char* presigned_url_str = obj["presignedUrl"];
        const char* s3_key_str = obj["key"]; // Get the S3 key

        if (presigned_url_str && s3_key_str) { // Check both are present
          Serial.printf("[MQTT] Enqueueing file %d/%d: Key='%s'\n", 
                        fileIndex, totalFilesInBatch, s3_key_str);
          // Serial.printf("[MQTT] URL: %s\n", presigned_url_str); // Uncomment for full URL if needed

          // *** MODIFIED CALL TO enqueueDownloadUrl ***
          enqueueDownloadUrl(presigned_url_str, s3_key_str); 
          // *****************************************

        } else {
            if (!presigned_url_str) Serial.printf("[MQTT] File %d/%d: 'presignedUrl' missing in JSON item.\n", fileIndex, totalFilesInBatch);
            if (!s3_key_str) Serial.printf("[MQTT] File %d/%d: 'key' missing in JSON item.\n", fileIndex, totalFilesInBatch);
        }
        fileIndex++;
      }
    } else {
      Serial.print("[MQTT] Failed to parse presigned URL JSON! Error: ");
      Serial.println(error.c_str());
    }
    return; // Successfully handled presigned URL, exit callback.
  } else {
    // ---- Detailed Logging for Presigned URL Mismatch ----
    if (strncmp(localTopicCopy, "/presignedurls/", 15) == 0) { 
        Serial.println(">>> PRESIGNED URL TOPIC MISMATCH (strcmp failed) <<<");
        // (Your detailed ASCII comparison logging can remain here if needed for debugging)
        // For brevity, I'll omit the long ASCII diff printout here, but you can keep it.
        Serial.println("   Raw Topic        : " + String(localTopicCopy));
        Serial.println("   Constructed Topic: " + presignedUrlTopic_constructed);
    }
  }

  // --- Registration Topic Check ---
  // REG_CHECK_TOPIC_SUB must be defined (e.g., in app.h or a config file)
  // Example: #define REG_CHECK_TOPIC_SUB "esp32/registration/status"
  if (strcmp(localTopicCopy, REG_CHECK_TOPIC_SUB) == 0) {
    Serial.println(">>> REGISTRATION TOPIC MATCH <<<");
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print("Received registration message payload: '"); Serial.print(message); Serial.println("'");

    // Using ArduinoJson for parsing registration payload as well for robustness
    StaticJsonDocument<256> regDoc; // Smaller doc for registration status
    DeserializationError regError = deserializeJson(regDoc, message);

    if (!regError) {
        const char* receivedDeviceId_json = regDoc["deviceId"];
        const char* status_json = regDoc["status"];
        String myDeviceId = getDeviceId(); // Get current device ID

        if (receivedDeviceId_json && status_json) {
            Serial.print("JSON - Received Device ID: '"); Serial.print(receivedDeviceId_json); Serial.println("'");
            Serial.print("JSON - Status: '"); Serial.print(status_json); Serial.println("'");
            Serial.print("My Device ID: '"); Serial.print(myDeviceId); Serial.println("'");

            if (strcmp(status_json, "linked") == 0 && myDeviceId == receivedDeviceId_json) {
                Serial.println("Device is LINKED and Device IDs match.");
                isDeviceRegistered = true;
                showDeviceLinked(); 
            } else if (strcmp(status_json, "not linked") == 0 && myDeviceId == receivedDeviceId_json) {
                Serial.println("Device is NOT LINKED and Device IDs match.");
                isDeviceRegistered = false;
                // Optionally show a 'not linked' message
            } else {
                Serial.println("Registration status/Device ID mismatch.");
            }
        } else {
            Serial.println("Registration JSON missing 'deviceId' or 'status'.");
        }
    } else {
        Serial.print("Failed to parse registration JSON! Error: ");
        Serial.println(regError.c_str());
        // Fallback to string parsing if needed, but JSON is preferred
    }
    return; 
  }
  
  // If the topic matched neither presigned URL nor registration check
  Serial.println("--- MQTT Message on Unhandled Topic ---");
  Serial.print("Topic: '"); Serial.print(localTopicCopy); Serial.println("'");
  Serial.print("Payload: '");
  for (unsigned int i = 0; i < length; i++) { Serial.print((char)payload[i]); }
  Serial.println("'");
  Serial.println("--- End Unhandled Topic ---");
}