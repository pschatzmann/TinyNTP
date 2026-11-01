// Tests of the generated TZ_* defines and of the NTP_* server defines
#include <fstream>
#include <regex>
#include <set>
#include <string>

#include "TinyNTP/TinyNTPServers.h"
#include "TinyNTP/TinyTimeZone.h"
#include "TinyNTP/TinyTimeZones.h"
#include "test.h"

#ifndef TINYNTP_SRC_DIR
#error "TINYNTP_SRC_DIR must point to the src directory"
#endif

using namespace tiny_ntp;

struct Define {
  std::string name, value;
};

static std::vector<Define> readDefines(const std::string& file,
                                       const std::string& prefix) {
  std::vector<Define> result;
  std::ifstream in(std::string(TINYNTP_SRC_DIR) + "/TinyNTP/" + file);
  std::regex re("^#define (" + prefix + "[A-Z0-9_]+)\\s+\"([^\"]*)\"");
  std::string line;
  while (std::getline(in, line)) {
    std::smatch m;
    if (std::regex_search(line, m, re)) result.push_back({m[1], m[2]});
  }
  return result;
}

TEST(all_time_zone_strings_can_be_parsed) {
  auto defines = readDefines("TinyTimeZones.h", "TZ_");
  CHECK(defines.size() > 400);
  int bad = 0;
  for (auto& d : defines) {
    TinyTimeZone z(d.value.c_str());
    if (!z.isValid()) {
      printf("  cannot parse %s = \"%s\"\n", d.name.c_str(), d.value.c_str());
      bad++;
    }
  }
  CHECK_EQ(bad, 0);
}

TEST(time_zone_names_are_unique_and_well_formed) {
  auto defines = readDefines("TinyTimeZones.h", "TZ_");
  std::set<std::string> names;
  std::regex re("^TZ_[A-Z0-9_]+$");
  for (auto& d : defines) {
    CHECK(std::regex_match(d.name, re));
    CHECK(names.insert(d.name).second);  // no duplicates
    CHECK(d.name.find("__") == std::string::npos);
    CHECK(!d.value.empty());
  }
}

TEST(time_zone_defines_have_the_expected_values) {
  CHECK_STR(TZ_EUROPE_ZURICH, "CET-1CEST,M3.5.0,M10.5.0/3");
  CHECK_STR(TZ_EUROPE_LONDON, "GMT0BST,M3.5.0/1,M10.5.0");
  CHECK_STR(TZ_AMERICA_NEW_YORK, "EST5EDT,M3.2.0,M11.1.0");
  CHECK_STR(TZ_ASIA_TOKYO, "JST-9");
  CHECK_STR(TZ_ASIA_KOLKATA, "IST-5:30");
  CHECK_STR(TZ_AUSTRALIA_SYDNEY, "AEST-10AEDT,M10.1.0,M4.1.0/3");
  // names with several parts, - and +
  CHECK_STR(TZ_AMERICA_ARGENTINA_BUENOS_AIRES, "<-03>3");
  CHECK_STR(TZ_AMERICA_PORT_AU_PRINCE, "EST5EDT,M3.2.0,M11.1.0");
  CHECK_STR(TZ_ETC_GMT_PLUS_5, "<-05>5");
  CHECK_STR(TZ_ETC_GMT_MINUS_14, "<+14>-14");
}

TEST(etc_gmt_defines_use_the_posix_sign) {
  // Etc/GMT+5 is UTC-5 (the sign is inverted in the IANA names)
  TinyTimeZone z(TZ_ETC_GMT_PLUS_5);
  CHECK_EQ(z.offsetSeconds(1800000000), -5 * 3600);
  TinyTimeZone e(TZ_ETC_GMT_MINUS_14);
  CHECK_EQ(e.offsetSeconds(1800000000), 14 * 3600);
}

TEST(ntp_ip_defines_are_ipv4_addresses) {
  auto defines = readDefines("TinyNTPServers.h", "NTP_IP_");
  CHECK(defines.size() >= 15);
  std::regex ip("^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$");
  std::set<std::string> seen;
  for (auto& d : defines) {
    std::smatch m;
    CHECK(std::regex_match(d.value, m, ip));
    for (int i = 1; i <= 4 && m.size() == 5; i++) CHECK(std::stoi(m[i]) <= 255);
    CHECK(seen.insert(d.name).second);
  }
}

TEST(ntp_host_defines_are_host_names) {
  auto defines = readDefines("TinyNTPServers.h", "NTP_HOST_");
  CHECK(defines.size() >= 15);
  std::regex host("^[a-z0-9]([a-z0-9.-]*[a-z0-9])?\\.[a-z]{2,}$");
  for (auto& d : defines) {
    if (!std::regex_match(d.value, host)) printf("  bad host %s\n", d.value.c_str());
    CHECK(std::regex_match(d.value, host));
  }
}

TEST(ntp_server_defines_have_the_expected_values) {
  CHECK_STR(NTP_HOST_POOL, "pool.ntp.org");
  CHECK_STR(NTP_HOST_GOOGLE, "time.google.com");
  CHECK_STR(NTP_HOST_CLOUDFLARE, "time.cloudflare.com");
  CHECK_STR(NTP_IP_GOOGLE_1, "216.239.35.0");
  CHECK_STR(NTP_IP_CLOUDFLARE_1, "162.159.200.1");
}

TEST_MAIN("defines")
