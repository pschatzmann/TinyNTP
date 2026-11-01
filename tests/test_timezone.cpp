// Tests of the POSIX TZ string parser and the conversion to local time
#include <cstdio>
#include <string>

#include "TinyNTP/TinyTimeZone.h"
#include "TinyNTP/TinyTimeZones.h"
#include "test.h"

using namespace tiny_ntp;

static std::string local(const TinyTimeZone& z, int64_t utc) {
  std::tm t = z.toLocal(utc);
  char b[40];
  snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900,
           t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  return b;
}

// 2026-07-01 12:00:00 UTC and 2026-01-15 12:00:00 UTC
static const int64_t summer = 1782907200, winter = 1768478400;

TEST(parse_valid_strings) {
  const char* valid[] = {
      "UTC0",
      "CET-1CEST,M3.5.0,M10.5.0/3",
      "EST5EDT,M3.2.0,M11.1.0",
      "EST5EDT",  // default rules
      "JST-9",
      "IST-5:30",
      "<+0330>-3:30",
      "<-03>3",
      "<+0545>-5:45",
      "AEST-10AEDT,M10.1.0,M4.1.0/3",
      "CET-1CEST,J90,J300/2",
      "CET-1CEST,60,300",
      "<-04>4<-03>,M9.1.6/24,M4.1.6/24",
      "GMT0BST,M3.5.0/1,M10.5.0",
      "XXX+3:30:15",
      "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0",
      "IST-1GMT0,M10.5.0,M3.5.0/1",
      "EST5EDT,M3.2.0/-1,M11.1.0/26",
  };
  for (const char* tz : valid) {
    TinyTimeZone z(tz);
    if (!z.isValid()) printf("  not valid: %s\n", tz);
    CHECK(z.isValid());
  }
}

TEST(parse_invalid_strings) {
  const char* invalid[] = {
      "",
      "CET",              // no offset
      "-1",               // no name
      "CE-1",             // name too short
      "CET-",             // no number
      "CET-1CEST,",       // rules missing
      "CET-1CEST,M3.5.0", // end rule missing
      "CET-1CEST,M13.1.0,M10.5.0",   // month 13
      "CET-1CEST,M0.1.0,M10.5.0",    // month 0
      "CET-1CEST,M3.0.0,M10.5.0",    // week 0
      "CET-1CEST,M3.6.0,M10.5.0",    // week 6
      "CET-1CEST,M3.5.7,M10.5.0",    // weekday 7
      "CET-1CEST,J0,J300",           // J starts at 1
      "CET-1CEST,J366,J300",
      "CET-1CEST,366,300",
      "CET-1CEST,M3.5.0,M10.5.0,x",  // trailing text
      "CET-1CEST,M3.5.0,M10.5.0/",   // empty time
      "<+03-3",                      // unterminated name
      "garbage,",
  };
  for (const char* tz : invalid) {
    TinyTimeZone z(tz);
    if (z.isValid()) printf("  valid: %s\n", tz);
    CHECK(!z.isValid());
  }
  TinyTimeZone n(nullptr);
  CHECK(!n.isValid());
}

TEST(invalid_string_falls_back_to_utc) {
  TinyTimeZone z("CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK(z.isValid());
  CHECK(!z.setTimeZone("nonsense"));
  CHECK(!z.isValid());
  CHECK(!z.hasDst());
  CHECK_STR(local(z, summer), "2026-07-01 12:00:00");  // UTC
  CHECK_STR(z.name(summer), "UTC");
  CHECK_EQ(z.offsetSeconds(summer), 0);
  TinyTimeZone d;  // default constructed
  CHECK(!d.isValid());
  CHECK_STR(local(d, winter), "2026-01-15 12:00:00");
}

TEST(zurich_summer_and_winter) {
  TinyTimeZone z(TZ_EUROPE_ZURICH);
  CHECK(z.hasDst());
  CHECK_STR(local(z, winter), "2026-01-15 13:00:00");
  CHECK_STR(local(z, summer), "2026-07-01 14:00:00");
  CHECK(!z.isDst(winter));
  CHECK(z.isDst(summer));
  CHECK_STR(z.name(winter), "CET");
  CHECK_STR(z.name(summer), "CEST");
  CHECK_EQ(z.offsetSeconds(winter), 3600);
  CHECK_EQ(z.offsetSeconds(summer), 7200);
  CHECK_EQ(z.toLocal(summer).tm_isdst, 1);
  CHECK_EQ(z.toLocal(winter).tm_isdst, 0);
}

TEST(zurich_transitions_to_the_second) {
  TinyTimeZone z(TZ_EUROPE_ZURICH);
  // 2026-03-29 01:00:00 UTC: 02:00 CET becomes 03:00 CEST
  CHECK_STR(local(z, 1774746000 - 1), "2026-03-29 01:59:59");
  CHECK_STR(local(z, 1774746000), "2026-03-29 03:00:00");
  CHECK(!z.isDst(1774746000 - 1));
  CHECK(z.isDst(1774746000));
  // 2026-10-25 01:00:00 UTC: 03:00 CEST becomes 02:00 CET
  CHECK_STR(local(z, 1792890000 - 1), "2026-10-25 02:59:59");
  CHECK_STR(local(z, 1792890000), "2026-10-25 02:00:00");
  CHECK(z.isDst(1792890000 - 1));
  CHECK(!z.isDst(1792890000));
}

TEST(southern_hemisphere) {
  TinyTimeZone z(TZ_AUSTRALIA_SYDNEY);
  CHECK_STR(local(z, summer), "2026-07-01 22:00:00");  // winter in Sydney
  CHECK(!z.isDst(summer));
  CHECK_STR(local(z, winter), "2026-01-15 23:00:00");  // summer in Sydney
  CHECK(z.isDst(winter));
  CHECK_STR(z.name(winter), "AEDT");
}

TEST(zones_without_summer_time) {
  TinyTimeZone tokyo(TZ_ASIA_TOKYO);
  CHECK(!tokyo.hasDst());
  CHECK_STR(local(tokyo, summer), "2026-07-01 21:00:00");
  CHECK(!tokyo.isDst(summer));
  TinyTimeZone india("IST-5:30");
  CHECK_STR(local(india, summer), "2026-07-01 17:30:00");
  TinyTimeZone nepal("<+0545>-5:45");
  CHECK_STR(local(nepal, summer), "2026-07-01 17:45:00");
  CHECK_STR(nepal.name(summer), "+0545");  // the <> are not part of the name
  TinyTimeZone west("<-03>3");
  CHECK_STR(local(west, summer), "2026-07-01 09:00:00");
}

TEST(offset_with_seconds) {
  TinyTimeZone z("XXX+3:30:15");  // west of UTC
  CHECK_EQ(z.offsetSeconds(summer), -(3 * 3600 + 30 * 60 + 15));
  CHECK_STR(local(z, summer), "2026-07-01 08:29:45");
}

TEST(rules_with_day_numbers) {
  // J60 is March 1st in all years (February 29th is not counted),
  // 59 is the 60th day of the year: February 29th in a leap year
  TinyTimeZone j("AAA0BBB,J60,J300");
  TinyTimeZone n("AAA0BBB,59,300");
  // 2027 (no leap year): J60 = March 1st, 59 = March 1st
  // 2028 (leap year): J60 = March 1st, 59 = February 29th
  int64_t feb29_2028_noon = 1835438400;  // 2028-02-29 12:00:00 UTC
  int64_t mar1_2028_noon = feb29_2028_noon + 86400;
  CHECK(!j.isDst(feb29_2028_noon));
  CHECK(j.isDst(mar1_2028_noon));
  CHECK(n.isDst(feb29_2028_noon));
  CHECK(n.isDst(mar1_2028_noon));
  int64_t feb28_2028_noon = feb29_2028_noon - 86400;
  CHECK(!n.isDst(feb28_2028_noon));
}

TEST(rule_time_is_local_time_of_the_old_zone) {
  // switch at 02:00 standard time (default) and at 03:00 summer time
  TinyTimeZone z("CET-1CEST,M3.5.0,M10.5.0/3");
  // 2027-03-28: the last Sunday of March 2027
  CHECK(!z.isDst(1806195600 - 1));  // 2027-03-28 00:59:59 UTC
  CHECK(z.isDst(1806195600));       // 01:00:00 UTC = 02:00 CET
  // 2027-10-31: back at 03:00 CEST = 01:00 UTC
  CHECK(z.isDst(1824944400 - 1));
  CHECK(!z.isDst(1824944400));
}

TEST(rule_times_beyond_24_hours_and_negative) {
  // Santiago: /24 means midnight of the next day
  TinyTimeZone s("<-04>4<-03>,M9.1.6/24,M4.1.6/24");
  CHECK(s.isValid());
  TinyTimeZone n("EST5EDT,M3.2.0/-1,M11.1.0/26");  // -1h and +26h
  CHECK(n.isValid());
  // start: March 8th 2026 at -01:00 standard time (the time replaces the
  // default 02:00) = March 7th 23:00 EST = March 8th 04:00 UTC
  CHECK(!n.isDst(1772942400 - 1));
  CHECK(n.isDst(1772942400));
  // end: November 1st 2026 + 26 hours = November 2nd 02:00 EDT = 06:00 UTC
  CHECK(n.isDst(1793599200 - 1));
  CHECK(!n.isDst(1793599200));
}

TEST(default_rules_are_the_us_rules) {
  TinyTimeZone d("EST5EDT");
  TinyTimeZone e("EST5EDT,M3.2.0,M11.1.0");
  for (int64_t t = 1735689600; t < 1767225600; t += 3600 * 5) {
    CHECK_EQ(d.isDst(t), e.isDst(t));
  }
}

TEST(dst_offset_can_be_given) {
  TinyTimeZone z("<+1030>-10:30<+11>-11,M10.1.0,M4.1.0");  // Lord Howe: 30 min
  CHECK_EQ(z.offsetSeconds(winter), 11 * 3600);
  CHECK_EQ(z.offsetSeconds(summer), 10 * 3600 + 1800);
  CHECK_STR(z.name(winter), "+11");
}

TEST(extreme_times) {
  TinyTimeZone z(TZ_EUROPE_ZURICH);
  CHECK_STR(local(z, 0), "1970-01-01 01:00:00");
  CHECK_STR(local(z, -1), "1970-01-01 00:59:59");
  CHECK_STR(local(z, 4102444800), "2100-01-01 01:00:00");
  CHECK_STR(local(z, 951782400), "2000-02-29 01:00:00");  // leap day
  CHECK_STR(local(z, 4107542400), "2100-03-01 01:00:00");  // 2100: no leap year
}

TEST(copy_keeps_the_rules) {
  TinyTimeZone a(TZ_EUROPE_ZURICH);
  TinyTimeZone b = a;
  a.setTimeZone("JST-9");
  CHECK_STR(local(b, summer), "2026-07-01 14:00:00");
  CHECK_STR(local(a, summer), "2026-07-01 21:00:00");
}

TEST(long_names_are_truncated) {
  TinyTimeZone z("LONGNAMEXX-1");
  CHECK(z.isValid());
  CHECK_EQ(std::string(z.name(summer)).size(), 7u);
  CHECK_EQ(z.offsetSeconds(summer), 3600);
}

// The expected values were calculated with python's zoneinfo (IANA data):
// see gen_timezone_cases.py
struct Case {
  const char* tz;
  int64_t utc;
  const char* local;
  int dst;
  const char* name;
};
static const Case cases[] = {
#include "timezone_cases.inc"
};

TEST(iana_oracle_21_zones_2025_to_2035) {
  int rows = 0;
  for (const Case& c : cases) {
    TinyTimeZone z(c.tz);
    std::string got = local(z, c.utc);
    bool ok = got == c.local && z.isDst(c.utc) == (c.dst != 0) &&
              std::string(z.name(c.utc)) == c.name;
    if (!ok) {
      printf("  %s utc=%lld: got %s %s dst=%d, expected %s %s dst=%d\n", c.tz,
             (long long)c.utc, got.c_str(), z.name(c.utc), z.isDst(c.utc),
             c.local, c.name, c.dst);
    }
    CHECK(ok);
    rows++;
  }
  CHECK(rows > 1000);
}

TEST_MAIN("timezone")
