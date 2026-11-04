// Tests of the NMEA parser and the GPS time source
#include <string>

#include "mock.h"
#include "test.h"
#include "TinyNTP/TinyGPSTimeSource.h"

using namespace tiny_ntp;

static const char* const kGga = "$GPGGA,123456.00,4717.1,N,00833.9,E,1,08,0.9,500.0,M,48.0,M,,*66\r\n";
static const char* const kGsvGps = "$GPGSV,3,1,11,01,40,083,46*47\r\n";
static const char* const kGsvGlonass = "$GLGSV,2,1,06,65,40,083,46*5E\r\n";
// 2026-11-04 12:34:56 UTC
static const char* const kRmc = "$GPRMC,123456.00,A,4717.1,N,00833.9,E,0.0,0.0,041126,,,A*5C\r\n";
static const char* const kRmcBad = "$GPRMC,123456.00,A,4717.1,N,00833.9,E,0.0,0.0,041126,,,A*00\r\n";
static const char* const kZda = "$GNZDA,123457.00,04,11,2026,00,00*7C\r\n";
static const uint64_t kRmcMs = 1793795696000ULL;

static int feed(TinyNMEAParser& p, const char* s) {
  int times = 0;
  for (const char* c = s; *c; ++c) times += p.feed(*c) ? 1 : 0;
  return times;
}

static void feed(TinyGPSTimeSource& g, const char* s) {
  for (const char* c = s; *c; ++c) g.feed(*c, micros());
}

TEST(parser_reads_time_and_satellites) {
  TinyNMEAParser p;
  CHECK_EQ(feed(p, kGga), 0);
  feed(p, kGsvGps);
  feed(p, kGsvGlonass);
  CHECK_EQ(feed(p, kRmc), 1);
  CHECK_EQ(p.getTimeMs(), kRmcMs);
  CHECK(p.hasFix());
  CHECK_EQ(p.getSatellitesUsed(), 8);
  CHECK_EQ(p.getSatellitesVisible(), 17);
  CHECK_EQ(feed(p, kZda), 1);
  CHECK_EQ(p.getTimeMs(), kRmcMs + 1000);
}

TEST(parser_ignores_wrong_checksums) {
  TinyNMEAParser p;
  CHECK_EQ(feed(p, kRmcBad), 0);
  CHECK_EQ(p.getTimeMs(), 0u);
  CHECK_EQ(p.getChecksumErrors(), 1u);
}

TEST(no_time_without_fix) {
  mock::reset();
  TinyGPSTimeSource g;
  CHECK_EQ(g.getTimeMs(), 0u);
  feed(g, "$GPRMC,123456.00,V,,,,,,,041126,,,N*7A\r\n");  // status V: no fix
  CHECK_EQ(g.getTimeMs(), 0u);
  CHECK_EQ(g.getFixCount(), 0u);
}

TEST(time_from_the_sentence_without_pps) {
  mock::reset();
  TinyGPSTimeSource g;
  g.setLatencyMs(100);
  feed(g, kRmc);
  CHECK_EQ(g.getFixCount(), 1u);
  CHECK_EQ(g.getTimeMs(), kRmcMs + 100);
  mock::now += 250;
  CHECK_EQ(g.getTimeMs(), kRmcMs + 350);
  uint64_t utc;
  uint32_t at;
  bool pps = true;
  CHECK(g.getLastFix(utc, at, pps));
  CHECK(!pps);
}

TEST(time_from_the_pps_edge) {
  mock::reset();
  TinyGPSTimeSource g;
  g.setLatencyMs(100);  // not used with PPS
  g.pps(micros());      // start of the second
  mock::now += 300;     // the sentence arrives 300 ms later
  feed(g, kRmc);
  CHECK(g.isPpsActive());
  CHECK_EQ(g.getTimeMs(), kRmcMs + 300);
  uint64_t utc;
  uint32_t at;
  bool pps = false;
  CHECK(g.getLastFix(utc, at, pps));
  CHECK(pps);
  CHECK_EQ(utc, kRmcMs);
  CHECK_EQ(at, 1000u * 1000u);  // micros() of the edge
}

TEST(same_second_is_counted_once) {
  mock::reset();
  TinyGPSTimeSource g;
  feed(g, kRmc);
  feed(g, kRmc);
  CHECK_EQ(g.getFixCount(), 1u);
}

TEST(old_fix_runs_with_millis) {
  mock::reset();
  TinyGPSTimeSource g;
  feed(g, kRmc);
  mock::now += 5 * 3600 * 1000UL;  // micros() has overflowed in the meantime
  CHECK_EQ(g.getTimeMs(), kRmcMs + 5 * 3600 * 1000ULL);
}

TEST_MAIN("gps")
