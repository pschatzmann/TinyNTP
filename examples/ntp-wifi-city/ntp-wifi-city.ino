// Example sketch for TinyNTPClient showing the local time of a city
// (incl. summer/winter time), e.g. on ESP32
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTPClient.h"

TinyNTPClient<WiFiUDP> ntp("82.197.188.130", 123, 1000);
const char* ssid = "SSID";
const char* password = "PASSWORD";
// The city for which we want to know the time
const char* city = "Europe/Zurich";
int updateInterval = 60;

// Dictionary of cities and their POSIX TZ strings. Add your own if needed.
const TinyTimeZoneEntry cities[] = {
    // Europe
    {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Istanbul", "<+03>-3"},
    {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Moscow", "MSK-3"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
    // America
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Mexico_City", "CST6"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Sao_Paulo", "<-03>3"},
    {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
    {"Pacific/Honolulu", "HST10"},
    // Asia
    {"Asia/Bangkok", "<+07>-7"},
    {"Asia/Dubai", "<+04>-4"},
    {"Asia/Hong_Kong", "HKT-8"},
    {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Singapore", "<+08>-8"},
    {"Asia/Seoul", "KST-9"},
    {"Asia/Tokyo", "JST-9"},
    // Africa
    {"Africa/Johannesburg", "SAST-2"},
    {"Africa/Lagos", "WAT-1"},
    {"Africa/Nairobi", "EAT-3"},
    // Oceania
    {"Australia/Perth", "AWST-8"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};
TinyTimeZoneDB timeZones(cities);

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

  // Look up the time zone of the city: "Zurich" would work as well
  const char* tz = timeZones.find(city);
  if (tz == nullptr) {
    Serial.print("Unknown city: ");
    Serial.println(city);
  } else if (!ntp.setTimeZone(tz)) {
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
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S ", &tm);
    Serial.print(city);
    Serial.print(": ");
    Serial.print(time_str);
    Serial.print(ntp.getTimeZoneName());  // e.g. CET or CEST
    Serial.println(tm.tm_isdst > 0 ? " (summer time)" : "");
  }
  delay(1000);  // Update every second
}
