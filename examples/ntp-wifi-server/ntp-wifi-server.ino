// Example sketch for TinyNTPServer using WiFi (e.g. on ESP32): the device gets
// the time from an NTP server in the internet and serves it to the other
// devices in the local network
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTP.h"

TinyNTPClient<WiFiUDP> ntp(NTP_IP_GOOGLE_1, 123, 3000);
TinyNTPServer<WiFiUDP> server;
const char* ssid = "SSID";
const char* password = "PASSWORD";
int updateInterval = 3600;  // seconds = 1h
uint32_t lastUpdate = 0;

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi connected: ");
  Serial.println(WiFi.localIP());

#ifdef ESP32
  // Without the WiFi power save the requests of the clients and the responses
  // of the internet server are not held back by the access point
  WiFi.setSleep(false);
#endif
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();

  // The server uses port 123: the client needs a different local port
  ntp.setLocalPort(12300);
  while (!ntp.begin()) {
    Serial.println("Failed to initialize NTP client");
    delay(5000);
  }
  Serial.println("NTP client initialized");

  // Serve the UTC time of the client (do not set a time offset or time zone
  // on this client). Stratum: the one of the upstream server plus 1
  server.setStratum(3);
  while (!server.begin(ntp)) {
    Serial.println("Failed to start NTP server");
    delay(5000);
  }
  Serial.println("NTP server initialized");
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
