// Example sketch for TinyNTPClient showing the local time (incl. summer/winter
// time) for a POSIX TZ string, e.g. on ESP32
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTPClient.h"

TinyNTPClient<WiFiUDP> ntp(NTP_IP_GOOGLE_1, 123, 1000);
const char* ssid = "SSID";
const char* password = "PASSWORD";
// POSIX TZ string for Europe/Zurich (CET-1CEST,M3.5.0,M10.5.0/3). All IANA
// zones are available as TZ_* defines, e.g. TZ_AMERICA_NEW_YORK
const char* tzString = TZ_EUROPE_ZURICH;
int updateInterval = 60;

void updateNTPTime() {
  if (updateInterval-- <= 0) {
    Serial.println("Updating NTP time...");
    if (!ntp.update()) {
      Serial.println("NTP update failed");
    }
    updateInterval = 60;  // reset update interval
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();

  // Initialize NTP client (the time stays in UTC: no offset is set)
  Serial.println("Starting NTP client...");
  if (!ntp.begin()) {
    Serial.println("Failed to initialize NTP client");
    return;
  }

  // Activate the time zone: getLocalTm() now applies the offset and DST.
  // The POSIX TZ string is parsed by the library, so no tzset() is needed.
  if (!ntp.setTimeZone(tzString)) {
    Serial.println("Invalid time zone string");
  }
}

void loop() {
  // Update NTP time every 60 seconds
  updateNTPTime();

  if (ntp) {
    // Local time with summer/winter time
    std::tm tm = ntp.getLocalTm();
    char time_str[90];
    strftime(time_str, sizeof(time_str), "Local time: %Y-%m-%d %H:%M:%S ", &tm);
    Serial.print(time_str);
    Serial.print(ntp.getTimeZoneName());  // CET or CEST
    Serial.println(tm.tm_isdst > 0 ? " (summer time)" : "");
  }
  delay(1000);  // Update every second
}
