/**
 * @file TinyNTP.h
 * @brief Include this file to use the library: NTP client and server, time
 * sources, time zones (POSIX TZ strings) and the defines for the NTP servers
 * and the time zones. All classes are in the namespace tiny_ntp, which is used
 * automatically if ARDUINO is defined.
 */

#pragma once

#include "TinyNTP/TinyClockTimeSource.h"
#include "TinyNTP/TinyNTPClient.h"
#include "TinyNTP/TinyNTPServer.h"
#include "TinyNTP/TinyNTPServers.h"
#include "TinyNTP/TinySHA256.h"
#include "TinyNTP/TinyTimeSet.h"
#include "TinyNTP/TinyTimeSource.h"
#include "TinyNTP/TinyTimeZone.h"
#include "TinyNTP/TinyTimeZones.h"

// The system clock needs gettimeofday(): it is not available on all boards
#if defined(__has_include)
#if __has_include(<sys/time.h>)
#include "TinyNTP/TinySystemTimeSource.h"
#endif
#endif

#if defined(ARDUINO)
// Arduino sketches use the classes without the namespace
using namespace tiny_ntp;
#endif
