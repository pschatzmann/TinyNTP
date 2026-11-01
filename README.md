# TinyNTP

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](https://www.gnu.org/licenses/gpl-3.0)

A minimal, header-only Network Time Protocol (NTP) client and server for Arduino and compatible platforms, designed for efficiency and portability. It works with any Arduino UDP API (e.g., WiFiUDP, EthernetUDP).


## Features

**Client**
- Retrieves accurate UTC time from NTP servers
- Minimal resource usage, suitable for embedded/IoT
- Works with any UDP API (WiFiUDP, EthernetUDP, etc.)
- Returns time as seconds, milliseconds, or `std::tm` struct
- `NTP_IP_*` / `NTP_HOST_*` defines for well known public NTP servers

**Time zones**
- Supports a fixed timezone offset
- Local time incl. summer/winter time from a POSIX TZ string (own parser, no `tzset()` needed)
- `TZ_*` defines for all IANA time zones (e.g. `TZ_EUROPE_ZURICH`)

**Server**
- `TinyNTPServer` serves the time to the devices in your network, from the NTP client, the system time (`TinySystemTimeSource`), or an RTC or GPS (`TinyClockTimeSource`); your own sources derive from `TinyTimeSource`

**General**
- `tinyntp`: command line tool for the desktop to query an NTP server, serve the time of your computer or set the time of a device (authenticated with HMAC-SHA256)
- Header-only, easy to integrate
- cmake support


## Quick Start

```cpp
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinyNTP.h"

TinyNTPClient<WiFiUDP> ntp(NTP_IP_GOOGLE_1, 123, 1000);

void setup() {
  Serial.begin(115200);
  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ntp.begin();                      // gets the time
  ntp.setTimeZone(TZ_EUROPE_ZURICH);  // optional: local time incl. summer time
}

void loop() {
  std::tm tm = ntp.getLocalTm();    // or getTm() for UTC
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  delay(1000);
}
```
Call `ntp.update()` from time to time (e.g. once per hour) to stay in sync.

Include `TinyNTP.h`: it provides everything. All classes are in the namespace `tiny_ntp`, and `TinyNTP.h` uses it (`using namespace tiny_ntp;`), so you can use the plain class names. If your code has classes with the same names, define `NO_TINY_NTP_NAMESPACE` before you include `TinyNTP.h` and write `tiny_ntp::TinyNTPClient<...>`. The individual headers in `src/TinyNTP/` do not use the namespace. The `NTP_*` and `TZ_*` defines are macros: they have no namespace.


## Installation in Arduino

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/TinyNTP.git
```

I recommend to use git because you can easily update to the latest version just by executing the git pull command in the project folder. 


## Documentation

- [Class Documentation](https://pschatzmann.github.io/TinyNTP/annotated.html)
- [UDP Stacks](https://github.com/pschatzmann/TinyNTP/wiki/UDP-Stacks): the supported networking stacks (WiFi, Ethernet, GSM, ...)
- [Time Zones](https://github.com/pschatzmann/TinyNTP/wiki/Time-Zones): fixed offset, POSIX TZ strings, the `TZ_*` defines and `TinyTimeZone`
- [Public NTP Servers](https://github.com/pschatzmann/TinyNTP/wiki/Public-NTP-Servers): the `NTP_IP_*` and `NTP_HOST_*` defines
- [NTP Server](https://github.com/pschatzmann/TinyNTP/wiki/NTP-Server): serve the time to your network with `TinyNTPServer`
- [Server Time Sources](https://github.com/pschatzmann/TinyNTP/wiki/Server-Time-Sources): NTP client, system time, GPS and RTC as source for the server
- [Command Line Tool](https://github.com/pschatzmann/TinyNTP/wiki/Command-Line-Tool): `tinyntp` for the desktop
- [Tests](https://github.com/pschatzmann/TinyNTP/wiki/Tests): the automatic tests

## Examples

- [ntp-wifi](https://github.com/pschatzmann/TinyNTP/blob/main/examples/ntp-wifi/ntp-wifi.ino): UTC time with a fixed offset
- [ntp-wifi-timezone](https://github.com/pschatzmann/TinyNTP/blob/main/examples/ntp-wifi-timezone/ntp-wifi-timezone.ino): local time with summer/winter time
- [ntp-wifi-server](https://github.com/pschatzmann/TinyNTP/blob/main/examples/ntp-wifi-server/ntp-wifi-server.ino): get the time from the internet and serve it to your network
- [ntp-wifi-server-rtc](https://github.com/pschatzmann/TinyNTP/blob/main/examples/ntp-wifi-server-rtc/ntp-wifi-server-rtc.ino): serve the time of an RTC (DS3231); needs the RTClib library and a board with an RTC

The examples (except the one with the RTC) can also be built as programs for the desktop with CMake and the [Arduino emulator](https://github.com/pschatzmann/arduino-emulator), which is downloaded from GitHub:

```bash
cmake -S examples -B build-examples && cmake --build build-examples
./build-examples/ntp-wifi-timezone/ntp-wifi-timezone
```
Each example directory also has its own `CMakeLists.txt`. On Linux, binding the UDP port 123 needs root: without it the emulator prints `could not bind socket: 13` (twice at the start), but the programs still work, because the system chooses a port when the request is sent. To avoid the messages run them with `sudo`, or call `ntp.setLocalPort()` with another port in the sketch.


## Tests

The library has automatic tests for the desktop (Linux, macOS), see the [Tests](https://github.com/pschatzmann/TinyNTP/wiki/Tests) page:

```bash
cmake -S tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```
They take about 40 seconds (most of it is waiting for the lifetime of a nonce in the tool test). Add `-DTINYNTP_EMULATOR_TESTS=ON` to also test with the Arduino emulator, which also compiles the examples.


## License

This library is licensed under the GNU GPL v3. See `License.txt` for details.


