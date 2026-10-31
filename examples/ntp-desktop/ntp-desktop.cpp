#include "Arduino.h"
#include "WiFi.h"
#include "WiFiUdp.h"

#include "TinyNTPClient.h"

TinyNTPClient<WiFiUDP> ntp(NTP_IP_GOOGLE_1);

void setup() {
  if (!ntp.begin()) {
    Serial.println("Failed to initialize NTP client");
    exit(1);
  }
}

void loop() {
  auto currentTime = ntp.getTimeSec();
  Serial.print("Current time (UTC): ");
  Serial.print( currentTime);

  auto tm = ntp.getTm();
  Serial.print(" / Formatted time (UTC): ");
  char buffer[80];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  Serial.println(buffer);
  delay(1000);  // Update every second
}