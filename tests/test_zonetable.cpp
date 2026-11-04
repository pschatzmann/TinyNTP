// Tests of the table of the IANA time zones
#include <cstdint>
#include <cstring>
#include <string>

#include "test.h"
#include "TinyNTP/TinyTimeZoneTable.h"
#include "TinyNTP/TinyTimeZones.h"

using namespace tiny_ntp;

TEST(finds_zones_by_name) {
  CHECK_STR(findTimeZone("Europe/Zurich"), TZ_EUROPE_ZURICH);
  CHECK_STR(findTimeZone("America/New_York"), TZ_AMERICA_NEW_YORK);
  CHECK_STR(findTimeZone("Africa/Abidjan"), TZ_AFRICA_ABIDJAN);  // first
  CHECK_STR(findTimeZone("Pacific/Wallis"), TZ_PACIFIC_WALLIS);  // last
}

TEST(unknown_zones) {
  CHECK(findTimeZone("Europe/Atlantis") == nullptr);
  CHECK(findTimeZone("") == nullptr);
  CHECK(findTimeZone(nullptr) == nullptr);
}

TEST(table_is_sorted_for_the_binary_search) {
  CHECK(tinyTimeZoneCount > 400);
  for (size_t i = 1; i < tinyTimeZoneCount; i++) {
    CHECK(strcmp(tinyTimeZoneTable[i - 1].name, tinyTimeZoneTable[i].name) < 0);
    CHECK(findTimeZone(tinyTimeZoneTable[i].name) == tinyTimeZoneTable[i].tz);
  }
}

TEST_MAIN("zonetable")
