// A "device" for the tests of the command line tool: a TinyNTPServer with a
// clock that has the wrong time. (main() is in device_main.cpp.)
//   test_device <port> <clock|fixed|nosetting> <seconds>
// clock: the time can be set with the secret "topsecret"
// fixed: time setting is on, but the time source cannot be set
// nosetting: time setting is off
#include <chrono>
#include <cstdlib>

#include "Arduino.h"
#include "EthernetUdp.h"

#include "TinyNTP.h"

struct FixedSource : TinyTimeSource {
  uint64_t getTimeMs() override { return 1800000000000ULL; }
};

int device_main(int argc, char** argv) {
  if (argc < 4) return 2;
  int port = atoi(argv[1]);
  char mode = argv[2][0];
  TinyClockTimeSource clock;
  clock.setTime(1609459200);  // 2021-01-01: wrong on purpose
  FixedSource fixed;
  TinyNTPServer<EthernetUDP> server;
  if (!server.begin(mode == 'f' ? static_cast<TinyTimeSource&>(fixed)
                                : static_cast<TinyTimeSource&>(clock),
                    port)) {
    return 3;
  }
  if (mode != 'n') server.enableTimeSetting("topsecret");
  auto end = std::chrono::steady_clock::now() +
             std::chrono::seconds(atoi(argv[3]));
  while (std::chrono::steady_clock::now() < end) {
    if (!server.process()) delay(1);  // parsePacket() does not wait
  }
  printf("ntp=%u timeset=%u\n", server.getRequestCount(),
         server.getTimeSetCount());
  return 0;
}
