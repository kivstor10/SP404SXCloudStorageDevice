#include "app.h"
#include "secrets.h"
#include "WiFi.h"


WiFiClientSecure net;
PubSubClient client(net);

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
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(messageHandler);

  while (!client.connected()) {
    Serial.print(".");
    if (client.connect(THINGNAME)) {
      Serial.println("Connected to AWS!");
      client.subscribe(REG_CHECK_TOPIC_SUB);
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