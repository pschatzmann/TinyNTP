// ARDUINO is defined (by the build): TinyNTP.h uses the namespace, so the
// classes can be used without it. All classes come with this one header.
#ifndef ARDUINO
#error "this test needs ARDUINO to be defined"
#endif
#include "mock.h"
#include "test.h"
#include "TinyNTP.h"

TEST(all_classes_are_available_without_namespace) {
  mock::reset();
  TinyNTPClient<MockUDP> client;
  TinyNTPServer<MockUDP> server;
  TinyClockTimeSource clock;
  TinySystemTimeSource system;
  TinyTimeZone zone(TZ_EUROPE_ZURICH);
  TinyTimeSource& source = clock;
  TinyTimeSetMessage::Status status = TinyTimeSetMessage::Ok;
  uint8_t digest[TinySHA256::DigestSize];
  TinySHA256 sha;
  sha.finish(digest);
  CHECK(zone.isValid());
  CHECK_EQ(source.getTimeMs(), 0u);
  CHECK_EQ(static_cast<int>(status), 0);
  CHECK_EQ(digest[0], 0xe3);  // SHA-256 of nothing
  CHECK(!client);
  (void)server;
  (void)system;
}

TEST(namespace_names_work_as_well) {
  tiny_ntp::TinyClockTimeSource clock;
  tiny_ntp::TinyTimeSource& source = clock;
  CHECK_EQ(source.getTimeMs(), 0u);
}

TEST(the_two_names_are_the_same_type) {
  tiny_ntp::TinyTimeZone a(TZ_ASIA_TOKYO);
  TinyTimeZone& b = a;  // would not compile for different types
  CHECK(b.isValid());
}

TEST_MAIN("namespace_arduino")
