// Example sketch for TinyNTPClient using WiFi (e.g. on ESP32)
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTPClient.h"

TinyNTPClient<WiFiUDP> ntp("82.197.188.130", 123, 1000);
const char* ssid = "SSID";
const char* password = "PASSWORD";
int updateInterval = 60;

void updateNTPTime(){
  if (updateInterval-- <= 0){ 
    Serial.println("Updating NTP time...");
    if (!ntp.update()) {
      Serial.println("NTP update failed");
    }
    Serial.print("Correction factor: ");
    Serial.println(ntp.getMillisCorrectionFactor(), 12);
    updateInterval = 60; // reset update interval
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

  // Initialize NTP client
  Serial.println("Starting NTP client...");
  if (!ntp.begin()) {
    Serial.println("Failed to initialize NTP client");
    return;
  }

  // Set time offset to +1 hour (e.g., for CET timezone) 
  ntp.setTimeOffsetHours(1); 
}

void loop() {
  // Update NTP time every 60 seconds
  updateNTPTime();

  if (ntp) {
    auto currentTime = ntp.getTimeSec();
    auto tm = ntp.getTm();
    char time_str[90];
    Serial.print("Current time (UTC): ");
    Serial.print(currentTime);
    Serial.print(" / Formatted time (UTC): ");
    sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900,
            tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    Serial.print(time_str);
  }
  delay(1000);  // Update every second
}