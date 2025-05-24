#include "app.h"
#include "WiFi.h"


void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    static unsigned long startAttemptTime = millis();
    if (millis() - startAttemptTime > 20000) {
      Serial.println("Connection error: Unable to connect to Wi-Fi within 20 seconds.");
      display.clearDisplay();
      display.setTextSize(1); 
      display.setCursor(10, 10); 
      display.print("Connection failed.");
      display.display();
    }
  }

  Serial.println("Connected to Wi-Fi!");
}

void connectAWS() {
  Serial.println("Connecting to AWS...");
  net.setCACert(AWS_CERT_CA);
  Serial.println("CA Cert set");
  net.setCertificate(AWS_CERT_CRT);
  Serial.println("Cert set");
  net.setPrivateKey(AWS_CERT_PRIVATE);
  Serial.println("Private Key set");

  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(messageHandler);

  while (!client.connected()) {
    Serial.print(".");
    if (client.connect(THINGNAME)) {
      Serial.println("Connected to AWS!");
      client.subscribe(REG_CHECK_TOPIC_SUB);
      Serial.println("Subscribed to " REG_CHECK_TOPIC_SUB);
      // Subscribe to presigned URLs topic using device ID
      String deviceId = getDeviceId();
      String presignedUrlTopic = "/presignedurls/" + deviceId;
      client.subscribe(presignedUrlTopic.c_str());
      Serial.print("Subscribed to: ");
      Serial.println(presignedUrlTopic);
    }
    else {
      Serial.print("Connect failed, rc=");
      Serial.println(client.state()); 
      delay(5000); // A delay before retrying
    }
  }
}

// Function to keep MQTT client active and handle incoming messages
void mqttLoop() {
  if (!client.connected()) {
      connectAWS(); // Reconnect if not connected
  }
  client.loop();  
}