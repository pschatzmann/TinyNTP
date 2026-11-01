// Example sketch for TinyNTPServer using WiFi (e.g. on ESP32): the device
// serves the time of an RTC (DS3231) to the other devices in the local network.
// Needs the RTClib library. The RTC must be set to UTC.
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTP.h"

RTC_DS3231 rtc;

// The clock of the server. A set request (tinyntp set) also sets the RTC
class RtcClockSource : public TinyClockTimeSource {
 public:
  bool setTimeMs(uint64_t unixMs) override {
    rtc.adjust(DateTime(static_cast<uint32_t>(unixMs / 1000)));
    return TinyClockTimeSource::setTimeMs(unixMs);
  }
};
RtcClockSource clockSource;
TinyNTPServer<WiFiUDP> server;
const char* ssid = "SSID";
const char* password = "PASSWORD";
uint32_t syncIntervalMs = 5UL * 60 * 1000;  // read the RTC every 5 minutes
uint32_t lastSync = 0;

// Read the RTC. The RTC has a resolution of one second: wait until the second
// changes, so that we know that we are at the start of the second.
void syncClock() {
  uint32_t sec = rtc.now().unixtime();
  uint32_t start = millis();
  while (rtc.now().unixtime() == sec && millis() - start < 1100) {
  }
  clockSource.setTime(rtc.now().unixtime(), 0);
  lastSync = millis();
}

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
  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (true) delay(1000);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power: the time is not valid");
  } else {
    syncClock();  // the time stays unknown (0) until it is set
  }
  connectToWiFi();

  // An RTC is not a reference clock: report a high stratum so that the
  // clients prefer other servers
  server.setStratum(10);
  server.begin(clockSource);
  // Optional: allow to set the time over the network with the command line
  // tool: tinyntp set <ip of this device> -k <secret>. Use a long random text.
  server.enableTimeSetting("CHANGE-THIS-SECRET");
}

void loop() {
  if (server.process()) {
    Serial.print("NTP requests served: ");
    Serial.println(server.getRequestCount());
  }
  // millis() is less exact than the RTC: read it again from time to time
  if (millis() - lastSync > syncIntervalMs && !rtc.lostPower()) {
    syncClock();
  }
}
