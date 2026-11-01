// Tests of the time sources: TinyClockTimeSource, TinySystemTimeSource and the
// base class TinyTimeSource
#include <chrono>

#include "mock.h"
#include "test.h"
#include "TinyNTP/TinyClockTimeSource.h"
#include "TinyNTP/TinySystemTimeSource.h"

using namespace tiny_ntp;

struct Fixed : TinyTimeSource {
  uint64_t t = 5;
  uint64_t getTimeMs() override { return t; }
};

TEST(base_class_cannot_be_set_by_default) {
  Fixed f;
  TinyTimeSource& base = f;
  CHECK_EQ(base.getTimeMs(), 5u);
  CHECK(!base.setTimeMs(1800000000000ULL));
  CHECK_EQ(base.getTimeMs(), 5u);
}

TEST(clock_is_not_set_at_the_start) {
  mock::reset();
  TinyClockTimeSource c;
  CHECK_EQ(c.getTimeMs(), 0u);
  CHECK(!c.isSet());
}

TEST(clock_runs_with_millis) {
  mock::reset();
  TinyClockTimeSource c;
  c.setTime(1800000000, 250);
  CHECK(c.isSet());
  CHECK_EQ(c.getTimeMs(), 1800000000250ULL);
  mock::now += 1;
  CHECK_EQ(c.getTimeMs(), 1800000000251ULL);
  mock::now += 999;
  CHECK_EQ(c.getTimeMs(), 1800000001250ULL);
  mock::now += 0;  // reading twice does not add time
  CHECK_EQ(c.getTimeMs(), 1800000001250ULL);
}

TEST(clock_can_be_set_again_and_cleared) {
  mock::reset();
  TinyClockTimeSource c;
  c.setTime(1800000000);
  mock::now += 5000;
  c.setTime(1900000000);  // the time since the first call is forgotten
  CHECK_EQ(c.getTimeMs(), 1900000000000ULL);
  c.setTimeMs(1900000000123ULL);
  CHECK_EQ(c.getTimeMs(), 1900000000123ULL);
  c.clear();
  CHECK_EQ(c.getTimeMs(), 0u);
  CHECK(!c.isSet());
  mock::now += 1000;
  CHECK_EQ(c.getTimeMs(), 0u);  // stays unknown
  c.setTime(1800000000);
  CHECK(c.isSet());
}

TEST(clock_setting_zero_clears_the_time) {
  mock::reset();
  TinyClockTimeSource c;
  c.setTime(1800000000);
  c.setTimeMs(0);
  CHECK(!c.isSet());
  CHECK_EQ(c.getTimeMs(), 0u);
}

TEST(clock_survives_the_overflow_of_millis) {
  mock::reset();
  TinyClockTimeSource c;
  mock::now = 0xFFFFFF00u;
  c.setTime(1800000000);
  mock::now = 0xFFFFFFF0u;
  CHECK_EQ(c.getTimeMs(), 1800000000000ULL + 240);
  mock::now = 0x00000100u;  // millis() started again at 0
  CHECK_EQ(c.getTimeMs(), 1800000000000ULL + 512);
  mock::now = 0x00000200u;
  CHECK_EQ(c.getTimeMs(), 1800000000000ULL + 768);
}

TEST(clock_survives_a_full_overflow_when_read_regularly) {
  mock::reset();
  TinyClockTimeSource c;
  c.setTime(1800000000);
  uint64_t expected = 1800000000000ULL;
  for (int i = 0; i < 500; i++) {  // 500 * 10 days
    mock::now += 864000000u;
    expected += 864000000u;
    CHECK_EQ(c.getTimeMs(), expected);
  }
}

TEST(clock_as_time_source_can_be_set_through_the_base_class) {
  mock::reset();
  TinyClockTimeSource c;
  TinyTimeSource& base = c;
  CHECK(base.setTimeMs(1800000000500ULL));
  CHECK_EQ(base.getTimeMs(), 1800000000500ULL);
}

struct RtcClock : TinyClockTimeSource {
  uint64_t rtc = 0;
  bool setTimeMs(uint64_t ms) override {
    rtc = ms;  // e.g. write the RTC
    return TinyClockTimeSource::setTimeMs(ms);
  }
};

TEST(derived_clock_can_write_an_rtc) {
  mock::reset();
  RtcClock c;
  TinyTimeSource& base = c;
  CHECK(base.setTimeMs(1800000000500ULL));
  CHECK_EQ(c.rtc, 1800000000500ULL);
  CHECK_EQ(c.getTimeMs(), 1800000000500ULL);
}

TEST(system_time_is_close_to_the_system_clock) {
  TinySystemTimeSource s;
  using namespace std::chrono;
  int64_t now = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()).count();
  CHECK_NEAR(static_cast<double>(s.getTimeMs()), static_cast<double>(now), 50);
}

TEST(system_time_before_the_minimum_is_not_known) {
  TinySystemTimeSource never(4000000000UL);  // the clock is before this time
  CHECK_EQ(never.getTimeMs(), 0u);
  TinySystemTimeSource always(0);
  CHECK(always.getTimeMs() > 0);
}

TEST_MAIN("timesources")
