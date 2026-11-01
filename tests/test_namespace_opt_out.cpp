// NO_TINY_NTP_NAMESPACE is defined: TinyNTP.h does not use the namespace. The
// classes are in tiny_ntp and do not disturb the code of the user, even if it
// has classes with the same names.
#define NO_TINY_NTP_NAMESPACE
#include "mock.h"
#include "test.h"

// the user has classes with the same names: this must not collide
struct TinyNTPClient { int mine = 1; };
struct TinyTimeZone { int mine = 2; };
struct TinyTimeSource { int mine = 3; };
struct TinyClockTimeSource { int mine = 4; };
struct TinyNTPServer { int mine = 5; };
struct TinySHA256 { int mine = 6; };

#include "TinyNTP.h"


TEST(user_classes_are_not_hidden) {
  TinyNTPClient a;
  TinyTimeZone b;
  TinyTimeSource c;
  TinyClockTimeSource d;
  TinyNTPServer e;
  TinySHA256 f;
  CHECK_EQ(a.mine + b.mine + c.mine + d.mine + e.mine + f.mine, 21);
}

TEST(library_classes_work_with_the_namespace) {
  mock::reset();
  tiny_ntp::TinyNTPClient<MockUDP> client;
  tiny_ntp::TinyNTPServer<MockUDP> server;
  tiny_ntp::TinyClockTimeSource clock;
  tiny_ntp::TinySystemTimeSource system;
  tiny_ntp::TinyTimeZone zone(TZ_EUROPE_ZURICH);
  tiny_ntp::TinyTimeSource& source = clock;
  clock.setTime(1782907200);
  CHECK(server.begin(source));
  CHECK(zone.isValid());
  CHECK(client.setTimeZone(TZ_ASIA_TOKYO));
  CHECK_EQ(client.getLocalTm().tm_hour, 9);  // 1970-01-01 00:00 UTC + 9 h
  (void)system;
}

TEST(macros_have_no_namespace) {
  CHECK_STR(TZ_EUROPE_ZURICH, "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(NTP_HOST_POOL, "pool.ntp.org");
}

TEST_MAIN("namespace_opt_out")
