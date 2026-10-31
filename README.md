# TinyNTPClient

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

A minimal, header-only Network Time Protocol (NTP) client for Arduino and compatible platforms, designed for efficiency and portability. It works with any Arduino UDP API (e.g., WiFiUDP, EthernetUDP).


## Features

- Retrieves accurate UTC time from NTP servers
- Minimal resource usage, suitable for embedded/IoT
- Header-only, easy to integrate
- Supports a fixed timezone offset
- Local time incl. summer/winter time from a POSIX TZ string (own parser, no `tzset()` needed)
- Optional city dictionary (e.g. `Europe/Zurich`)
- Works with any UDP API (WiFiUDP, EthernetUDP, etc.)
- Returns time as seconds, milliseconds, or `std::tm` struct
- cmake support

## Installation in Arduino

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/TinyNTPClient.git
```

I recommend to use git because you can easily update to the latest version just by executing the git pull command in the project folder. 


## Time Zones

By default the time is UTC. There are 3 ways to get the local time:

### Fixed offset
```cpp
ntp.setTimeOffsetHours(1);   // or setTimeOffsetSeconds()
auto tm = ntp.getTm();
```
There is no summer/winter time handling: you need to change the offset yourself.

### POSIX TZ string
The library contains its own parser for [POSIX TZ strings](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03), so it works on any platform and does not depend on `tzset()` or `localtime_r()`. Summer/winter time is switched automatically:
```cpp
ntp.setTimeZone("CET-1CEST,M3.5.0,M10.5.0/3");   // Europe/Zurich
std::tm tm = ntp.getLocalTm();                   // tm.tm_isdst is set in summer time
const char* name = ntp.getTimeZoneName();        // "CET" or "CEST"
```
The string reads: standard time name and offset (the sign is inverted: `-1` means UTC+1), the summer time name, and the rules for the start and the end of summer time. `M3.5.0` is the last (5) Sunday (0) in March (3), `/3` is the time of day (default 02:00). Use either the fixed offset or the time zone, not both.

The strings for all zones can be found in [posix_tz_db](https://github.com/nayarsystems/posix_tz_db). A POSIX string contains only the current rules, so dates before the last rule change of a country can be wrong.

### City dictionary
`TinyTimeZoneDB` maps city names to POSIX TZ strings. You provide the array, so you only pay for the cities that you need:
```cpp
const TinyTimeZoneEntry cities[] = {
    {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"Asia/Tokyo", "JST-9"},
};
TinyTimeZoneDB timeZones(cities);

ntp.setTimeZone(timeZones.find("Europe/Zurich"));  // "Zurich" works as well
```
`find()` ignores the case and returns `nullptr` for an unknown city.

## Documentaion

- [Class Documentation](https://pschatzmann.github.io/TinyNTPClient/annotated.html) 
- [Example Sketch](https://github.com/pschatzmann/TinyNTPClient/blob/main/examples/ntp-wifi/ntp-wifi.ino)
- [Example Sketch with POSIX TZ string](https://github.com/pschatzmann/TinyNTPClient/blob/main/examples/ntp-wifi-timezone/ntp-wifi-timezone.ino)
- [Example Sketch with city dictionary](https://github.com/pschatzmann/TinyNTPClient/blob/main/examples/ntp-wifi-city/ntp-wifi-city.ino)


## License

This library is licensed under the GNU GPL v3. See `License.txt` for details.


