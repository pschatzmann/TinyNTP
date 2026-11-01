// Example sketch for TinyNTPServer using WiFi (e.g. on ESP32): the device gets
// the time from an NTP server in the internet and serves it to the other
// devices in the local network
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTPClient.h"
#include "TinyNTPServer.h"

TinyNTPClient<WiFiUDP> ntp(NTP_IP_GOOGLE_1, 123, 1000);
TinyNTPServer<WiFiUDP> server;
const char* ssid = "SSID";
const char* password = "PASSWORD";
int updateInterval = 3600;  // seconds
uint32_t lastUpdate = 0;

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();

  // The server uses port 123: the client needs a different local port
  ntp.setLocalPort(12300);
  if (!ntp.begin()) {
    Serial.println("Failed to initialize NTP client");
    return;
  }

  // Serve the UTC time of the client (do not set a time offset or time zone
  // on this client). Stratum: the one of the upstream server plus 1
  server.setStratum(3);
  if (!server.begin(ntp)) {
    Serial.println("Failed to start NTP server");
  }
}

void loop() {
  // Answer the requests of the NTP clients
  if (server.process()) {
    Serial.print("NTP requests served: ");
    Serial.println(server.getRequestCount());
  }

  // Update the time from the internet
  if (millis() - lastUpdate > updateInterval * 1000UL) {
    lastUpdate = millis();
    if (!ntp.update()) {
      Serial.println("NTP update failed");
    }
  }
}
