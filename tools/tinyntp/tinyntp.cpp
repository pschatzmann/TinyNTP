// tinyntp: command line NTP client and server for the desktop (Linux, macOS)
// based on the TinyNTP library and the Arduino emulator. IPv4 only.
//
//   tinyntp [options] [server]     query a server and show the time
//   tinyntp serve [options]        serve the time of this computer
//   tinyntp set [options] device   set the time of a TinyNTPServer
//
// (main() is in main.cpp.)
//
// Build: cmake -S tools/tinyntp -B build-tinyntp && cmake --build build-tinyntp
#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// The Arduino API for the desktop (millis(), delay(), Serial, EthernetUDP) comes
// from the Arduino emulator
#include "Arduino.h"
#include "EthernetUdp.h"

#include "TinyNTP.h"

static bool verbose = false;

/// Discards everything that is written to std::cout, which is where Serial of
/// the emulator prints: the diagnostic messages of the library and the emulator
struct NullBuffer : std::streambuf {
  int overflow(int c) override { return c; }
};

// ---- command line -----------------------------------------------------------
static const char* usage =
    "tinyntp: NTP client and server based on the TinyNTP library\n"
    "\n"
    "Usage:\n"
    "  tinyntp [options] [server]   show the time of an NTP server\n"
    "  tinyntp serve [options]      serve the time of this computer\n"
    "  tinyntp set [options] <device>\n"
    "                               set the time of a device that runs a\n"
    "                               TinyNTPServer with enableTimeSetting()\n"    "\n"
    "Options:\n"
    "  -p <port>      port of the server (default: 123)\n"
    "  -t <ms>        time to wait for the response (default: 3000)\n"
    "  -z <tz>        POSIX TZ string to show the local time, e.g.\n"
    "                 \"CET-1CEST,M3.5.0,M10.5.0/3\" (Europe/Zurich)\n"
    "  -s <stratum>   serve: the stratum that is reported (default: 3)\n"
    "  -k <secret>    set: the shared secret of the device (or use the\n"
    "                 environment variable TINYNTP_KEY: not visible in ps)\n"    "  -v             show diagnostic messages\n"
    "  -h             show this help\n"
    "\n"
    "The server is a host name or an IPv4 address (default: pool.ntp.org).\n"
    "'serve' needs the right to use the port (123: run it as root, or use -p).\n"
    "It serves the clock of this computer: make sure that it is synchronized.\n"
    "'set' sends the clock of this computer, corrected by half of the round trip\n"
    "time. The device accepts it only with the right secret (HMAC-SHA256).\n";

static volatile sig_atomic_t stopRequested = 0;
static void onSignal(int) { stopRequested = 1; }

static void printTime(const char* label, const std::tm& tm, uint64_t ms,
                      const char* suffix) {
  printf("%-14s %04d-%02d-%02d %02d:%02d:%02d.%03d%s\n", label,
         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
         tm.tm_sec, static_cast<int>(ms % 1000), suffix);
}

static int runQuery(const char* server, int port, uint32_t timeoutMs,
                    const char* tz) {
  TinyNTPClient<EthernetUDP> ntp(server, port, timeoutMs);
  // an unprivileged local port: port 123 is used by the system or needs root
  ntp.setLocalPort(49152 + getpid() % 16000);
  if (tz != nullptr && !ntp.setTimeZone(tz)) {
    fprintf(stderr, "invalid time zone string: %s\n", tz);
    return 2;
  }
  if (!ntp.begin()) {
    fprintf(stderr, "no response from %s:%d (waited %u ms)\n", server, port,
            timeoutMs);
    return 1;
  }

  uint64_t ms = ntp.getTimeMs();
  std::tm utc = ntp.getTm();
  printf("%-14s %s:%d\n", "server:", server, port);
  printTime("UTC:", utc, ms, "");
  if (tz != nullptr) {
    std::tm local = ntp.getLocalTm();
    char suffix[40];
    snprintf(suffix, sizeof suffix, " %s%s", ntp.getTimeZoneName(),
             local.tm_isdst > 0 ? " (summer time)" : "");
    printTime("local:", local, ms, suffix);
  }
  using namespace std::chrono;
  int64_t system = duration_cast<milliseconds>(
                       system_clock::now().time_since_epoch())
                       .count();
  int64_t diff = static_cast<int64_t>(ms) - system;
  printf("%-14s %+.3f s (server - this computer)\n", "clock offset:",
         diff / 1000.0);
  return 0;
}

static int64_t systemTimeMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

/// Wait for a message of the device. Returns its size or 0 on timeout.
static int receive(EthernetUDP& udp, uint8_t* buffer, int size,
                   uint32_t timeoutMs) {
  uint32_t start = millis();
  while (static_cast<uint32_t>(millis() - start) < timeoutMs) {
    int n = udp.parsePacket();
    if (n > 0) return udp.read(buffer, n < size ? n : size);
  }
  return 0;
}

static int runSet(const char* device, int port, uint32_t timeoutMs,
                  const char* key) {
  using Msg = TinyTimeSetMessage;
  const uint8_t* k = reinterpret_cast<const uint8_t*>(key);
  size_t keyLen = strlen(key);
  EthernetUDP udp;
  if (!udp.begin(49152 + getpid() % 16000)) {
    fprintf(stderr, "cannot open a local UDP port\n");
    return 1;
  }
  uint8_t out[Msg::SetRequestSize], in[64];

  // 1. ask for a nonce and measure the round trip time
  using Clock = std::chrono::steady_clock;
  auto t0 = Clock::now();
  if (!udp.beginPacket(device, port)) {
    fprintf(stderr, "cannot resolve %s\n", device);
    return 1;
  }
  size_t n = Msg::makeChallengeRequest(out);
  udp.write(out, n);
  udp.endPacket();
  uint8_t nonce[Msg::NonceSize];
  int len = receive(udp, in, sizeof in, timeoutMs);
  auto t1 = Clock::now();
  if (len == 0 || !Msg::parseChallengeResponse(in, len, nonce)) {
    fprintf(stderr,
            "no response from %s:%d (waited %u ms): is this a TinyNTPServer "
            "with enableTimeSetting()?\n",
            device, port, timeoutMs);
    return 1;
  }
  double rttMs =
      std::chrono::duration<double, std::milli>(t1 - t0).count();

  // 2. send the time of this computer plus the time that the request needs
  uint64_t timeMs = systemTimeMs() + static_cast<uint64_t>(rttMs / 2 + 0.5);
  n = Msg::makeSetRequest(out, k, keyLen, nonce, timeMs);
  udp.beginPacket(device, port);
  udp.write(out, n);
  udp.endPacket();
  len = receive(udp, in, sizeof in, timeoutMs);
  Msg::Status status;
  uint64_t deviceMs = 0;
  if (len == 0 || !Msg::parseSetResponse(in, len, status, deviceMs)) {
    fprintf(stderr, "no response to the set request\n");
    return 1;
  }

  printf("%-14s %s:%d\n", "device:", device, port);
  switch (status) {
    case Msg::Ok: {
      time_t sec = deviceMs / 1000;
      std::tm tm;
      gmtime_r(&sec, &tm);
      printTime("time set:", tm, deviceMs, " UTC");
      printf("%-14s %.1f ms (half of it is added to the time)\n",
             "round trip:", rttMs);
      printf("%-14s %+.3f s (device - this computer)\n", "clock offset:",
             (static_cast<int64_t>(deviceMs) - systemTimeMs()) / 1000.0);
      return 0;
    }
    case Msg::Rejected:
      fprintf(stderr, "rejected: wrong secret\n");
      return 3;
    case Msg::NotSupported:
      fprintf(stderr, "the time source of the device cannot be set\n");
      return 3;
    case Msg::InvalidTime:
      fprintf(stderr, "the device refused the time (before 2020)\n");
      return 3;
  }
  return 3;
}

static int runServe(int port, int stratum) {
  TinySystemTimeSource source;
  TinyNTPServer<EthernetUDP> server;
  server.setStratum(stratum);
  if (!server.begin(source, port)) {
    fprintf(stderr, "cannot listen on UDP port %d%s\n", port,
            port < 1024 ? " (ports below 1024 need root: try -p 12345)" : "");
    return 1;
  }
  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);
  printf("serving the time of this computer on UDP port %d (stratum %d), "
         "stop with ctrl-c\n",
         port, stratum);
  fflush(stdout);
  while (!stopRequested) {
    if (server.process()) {
      if (verbose) fprintf(stderr, "request %u\n", server.getRequestCount());
    } else {
      delay(1);  // parsePacket() does not wait: do not use all of the CPU
    }
  }
  printf("\nserved %u requests\n", server.getRequestCount());
  return 0;
}

int tinyntp_main(int argc, char** argv) {
  bool serve = false, set = false, haveServer = false;
  const char* key = getenv("TINYNTP_KEY");
  const char* server = "pool.ntp.org";
  const char* tz = nullptr;
  int port = 123, stratum = 3;
  uint32_t timeoutMs = 3000;

  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    auto value = [&]() -> const char* {
      if (i + 1 >= argc) {
        fprintf(stderr, "missing value for %s\n", a);
        exit(2);
      }
      return argv[++i];
    };
    if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      fputs(usage, stdout);
      return 0;
    } else if (!strcmp(a, "-v")) {
      verbose = true;
    } else if (!strcmp(a, "-p")) {
      port = atoi(value());
    } else if (!strcmp(a, "-t")) {
      timeoutMs = atoi(value());
    } else if (!strcmp(a, "-z")) {
      tz = value();
    } else if (!strcmp(a, "-s")) {
      stratum = atoi(value());
    } else if (!strcmp(a, "-k")) {
      key = value();
    } else if (!strcmp(a, "serve")) {
      serve = true;
    } else if (!strcmp(a, "set")) {
      set = true;
    } else if (a[0] == '-') {
      fprintf(stderr, "unknown option %s\n\n%s", a, usage);
      return 2;
    } else {
      server = a;
      haveServer = true;
    }
  }
  static NullBuffer nullBuffer;
  if (!verbose) std::cout.rdbuf(&nullBuffer);  // the tool prints with printf
  if (port < 1 || port > 65535 || stratum < 1 || stratum > 15 ||
      timeoutMs == 0) {
    fprintf(stderr, "invalid option value\n\n%s", usage);
    return 2;
  }
  if (set) {
    if (key == nullptr || key[0] == 0) {
      fprintf(stderr, "set needs the secret: use -k or TINYNTP_KEY\n");
      return 2;
    }
    if (!haveServer) {
      fprintf(stderr, "set needs the address of the device\n");
      return 2;
    }
    return runSet(server, port, timeoutMs, key);
  }
  return serve ? runServe(port, stratum)
               : runQuery(server, port, timeoutMs, tz);
}
