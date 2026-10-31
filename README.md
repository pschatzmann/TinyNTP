# TinyNTPClient

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](https://www.gnu.org/licenses/gpl-3.0)

A minimal, header-only Network Time Protocol (NTP) client for Arduino and compatible platforms, designed for efficiency and portability. It works with any Arduino UDP API (e.g., WiFiUDP, EthernetUDP).


## Features

- Retrieves accurate UTC time from NTP servers
- Minimal resource usage, suitable for embedded/IoT
- Header-only, easy to integrate
- `NTP_IP_*` / `NTP_HOST_*` defines for well known public NTP servers
- Supports a fixed timezone offset
- Local time incl. summer/winter time from a POSIX TZ string (own parser, no `tzset()` needed)
- `TZ_*` defines for all IANA time zones (e.g. `TZ_EUROPE_ZURICH`)
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


## Documentaion

- [Class Documentation](https://pschatzmann.github.io/TinyNTPClient/annotated.html) 
- [Example Sketch](https://github.com/pschatzmann/TinyNTPClient/blob/main/examples/ntp-wifi/ntp-wifi.ino)
- [UDP Stacks](https://github.com/pschatzmann/TinyNTPClient/wiki/UDP-Stacks): the supported networking stacks (WiFi, Ethernet, GSM, ...)
- [Time Zones](https://github.com/pschatzmann/TinyNTPClient/wiki/Time-Zones): fixed offset, POSIX TZ strings and the `TZ_*` defines


## License

This library is licensed under the GNU GPL v3. See `License.txt` for details.


