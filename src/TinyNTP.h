/**
 * @file TinyNTP.h
 * @brief Include this file to use the library: NTP client and server, time
 * sources, time zones (POSIX TZ strings) and the defines for the NTP servers
 * and the time zones. All classes are in the namespace tiny_ntp. This header
 * uses the namespace (using namespace tiny_ntp;), so that the classes can be
 * used without it. Define NO_TINY_NTP_NAMESPACE before the include to switch
 * this off, e.g. if your code has classes with the same names.
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

#if !defined(NO_TINY_NTP_NAMESPACE)
using namespace tiny_ntp;
#endif
