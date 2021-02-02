#include <ESP8266WiFi.h>

#include <TOGoSStreamOperators.h>
#include <TOGoSCommand.h>
#include <TOGoS/Command/MQTTMaintainer.h>

#include "secrets.h"

using MQTTMaintainer = TOGoS::Command::MQTTMaintainer;
using StringView = TOGoS::StringView;

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
MQTTMaintainer mqttMaintainer = MQTTMaintainer::makeStandard(
  &pubSubClient,
  "mqtt-maintenance-demo", // client ID; maybe use MAC address instead?
  "mqtt-maintenance-demo"
);
  
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("# Hello, welcome to MQTT Maintenance demo thing\n");
  // TODO: setup based on a serial command so that we don't need to store username/password in here!
  // Of course you should have the option to put a default username/password in firmware, I guess.
  mqttMaintainer.setServer(
    StringView(MQTT_SERVER), uint16_t(1883),
    StringView(MQTT_USERNAME), StringView(MQTT_PASSWORD)
  );

  Serial << "# Connecting to " << WIFI_SSID << "...\n";
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Interval after which we'll re-report old info to Serial
long reReportInterval = 60000;

class WiFiWatcher {
  bool wasConnected = false;
  long lastReport = 0;
public:
  void update(long currentTime) {
    bool connected = (WiFi.status() == WL_CONNECTED);
    if( connected != wasConnected || currentTime - lastReport >= reReportInterval ) {
      Serial << "wifi " << (connected ? "connected" : "disconnected") << "\n";
      if( connected ) {
	Serial << "wifi/ip-address " << WiFi.localIP() << "\n";
      }
      wasConnected = connected;
      lastReport = currentTime;
    }
  }
} wifiWatcher;
  
long lastReport = 0;
bool wasMqttConnected = false;

void loop() {
  long currentTime = millis();

  wifiWatcher.update(currentTime);
  
  bool mqttConnected = mqttMaintainer.update();
  if( mqttConnected != wasMqttConnected || currentTime - lastReport > reReportInterval ) {
    wasMqttConnected = mqttConnected;
    if( mqttConnected ) {
      Serial << "mqtt connected\n";
    } else {
      Serial << "mqtt disconnected\n";
    }
    lastReport = currentTime;
  }
  delay(500);
}
