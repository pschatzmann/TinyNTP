#include "zones.h"

#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "TinyNTP/TinyTimeZone.h"
#include "ZoneTable.h"

using tiny_ntp::TinyTimeZone;

namespace {

std::string lower(std::string s) {
  for (auto& c : s) c = c == ' ' ? '_' : std::tolower(static_cast<unsigned char>(c));
  return s;
}

std::string cityOf(const std::string& name) {
  size_t p = name.rfind('/');
  return p == std::string::npos ? name : name.substr(p + 1);
}

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
  return s.substr(i);
}

bool validTz(const std::string& tz) { return TinyTimeZone(tz.c_str()).isValid(); }

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// The name after "zoneinfo/" in a path, e.g. /usr/share/zoneinfo/Europe/Zurich
std::string nameFromPath(const std::string& path) {
  size_t p = path.rfind("zoneinfo/");
  return p == std::string::npos ? std::string() : path.substr(p + 9);
}

const char* zoneinfoDirs[] = {"/usr/share/zoneinfo/", "/usr/lib/zoneinfo/",
                              "/usr/share/lib/zoneinfo/",
                              "/var/db/timezone/zoneinfo/"};

/// The POSIX TZ string at the end of a TZif file (RFC 8536, version 2 and later)
std::string tzifFooter(const std::string& path) {
  std::string data = readFile(path);
  if (data.size() < 5 || data.compare(0, 4, "TZif") != 0 || data[4] < '2') return "";
  while (!data.empty() && data.back() == '\n') data.pop_back();
  size_t p = data.rfind('\n');
  if (p == std::string::npos) return "";
  std::string footer = data.substr(p + 1);
  return validTz(footer) ? footer : "";
}

bool inTable(const std::string& name, std::string* tz = nullptr) {
  std::string n = lower(name);
  for (const ZoneEntry& e : zoneTable) {
    if (lower(e.name) == n) {
      if (tz) *tz = e.tz;
      return true;
    }
  }
  return false;
}

/// Compares the string with the local time of this computer for the next 9
/// years: a POSIX TZ string contains only the current rules, so the past is not
/// compared. Returns the number of different times (-1 if the string is invalid).
int compare(const std::string& tz, int* samples = nullptr) {
  TinyTimeZone z(tz.c_str());
  if (!z.isValid()) return -1;
  int n = 0, diff = 0;
  time_t start = time(nullptr);
  for (time_t t = start; t < start + 9 * 365 * 86400L; t += 6 * 3600) {  // every 6 hours
    std::tm local;
    localtime_r(&t, &local);
    if (z.offsetSeconds(t) != local.tm_gmtoff) diff++;
    n++;
  }
  if (samples) *samples = n;
  return diff;
}

/// A quick test with four times in the next year (all seasons) before the
/// complete one
bool quickMatch(const std::string& tz) {
  TinyTimeZone z(tz.c_str());
  for (int i = 0; i < 4; i++) {
    time_t t = time(nullptr) + i * 91L * 86400;
    std::tm local;
    localtime_r(&t, &local);
    if (z.offsetSeconds(t) != local.tm_gmtoff) return false;
  }
  return true;
}

}  // namespace

std::vector<Zone> findZones(const std::string& text) {
  std::vector<Zone> all, exact, city, part;
  std::string t = lower(trim(text));
  for (const ZoneEntry& e : zoneTable) {
    Zone z{e.name, e.tz};
    all.push_back(z);
    std::string n = lower(z.name);
    if (n == t) exact.push_back(z);
    else if (lower(cityOf(z.name)) == t) city.push_back(z);
    else if (n.find(t) != std::string::npos) part.push_back(z);
  }
  if (t.empty()) return all;
  if (!exact.empty()) return exact;
  if (!city.empty()) return city;
  return part;
}

bool resolveZone(const std::string& arg, std::string& tz, std::string& error) {
  if (arg == "local") {
    LocalZone local = findLocalZone();
    if (local.tz.empty()) {
      error = "cannot find the time zone of this computer";
      return false;
    }
    tz = local.tz;
    return true;
  }
  if (validTz(arg)) {  // a POSIX TZ string
    tz = arg;
    return true;
  }
  std::vector<Zone> zones = findZones(arg);
  if (zones.size() == 1) {
    tz = zones[0].tz;
    return true;
  }
  error = zones.empty() ? "unknown time zone: " + arg + " (not a POSIX TZ string, not a zone name)"
                        : "the time zone is not unique: " + arg + " (use tinyntp tz " + arg + ")";
  return false;
}

LocalZone findLocalZone() {
  LocalZone r;

  // 1. the name of the zone
  if (const char* env = getenv("TZ")) {
    std::string v = env;
    if (!v.empty() && v[0] == ':') v = v.substr(1);
    if (!v.empty()) {
      bool file = false;
      for (const char* dir : zoneinfoDirs) file = file || access((std::string(dir) + v).c_str(), R_OK) == 0;
      if (inTable(v) || file) {
        r.name = v;
        r.nameFrom = "the TZ variable";
      } else if (validTz(v)) {
        r.tz = v;
        r.tzFrom = "the TZ variable";
      }
    }
  } else {
    char link[512];
    ssize_t n = readlink("/etc/localtime", link, sizeof link - 1);
    if (n > 0) {
      link[n] = 0;
      r.name = nameFromPath(link);
      if (!r.name.empty()) r.nameFrom = "/etc/localtime";
    }
    if (r.name.empty()) {
      std::string tzFile = trim(readFile("/etc/timezone"));
      if (!tzFile.empty() && tzFile.find('\n') == std::string::npos) {
        r.name = tzFile;
        r.nameFrom = "/etc/timezone";
      }
    }
  }

  // 2. the POSIX TZ string: the footer of the zoneinfo file is the most exact
  if (r.tz.empty() && !r.name.empty()) {
    for (const char* dir : zoneinfoDirs) {
      r.tz = tzifFooter(std::string(dir) + r.name);
      if (!r.tz.empty()) {
        r.tzFrom = "the zoneinfo file";
        break;
      }
    }
  }
  if (r.tz.empty() && !getenv("TZ")) {
    r.tz = tzifFooter("/etc/localtime");
    if (!r.tz.empty()) r.tzFrom = "/etc/localtime";
  }
  if (r.tz.empty() && !r.name.empty() && inTable(r.name, &r.tz)) {
    r.tzFrom = "the table of the library";
  }

  // 3. nothing found: compare the local time of this computer with the table
  if (r.tz.empty()) {
    for (const ZoneEntry& e : zoneTable) {
      if (quickMatch(e.tz) && compare(e.tz) == 0) {
        r.tz = e.tz;
        r.tzFrom = "comparing the local time with the table";
        break;  // the name stays unknown: several zones have the same rules
      }
    }
  }

  // the check: does the string give the local time of this computer?
  if (!r.tz.empty()) {
    r.differences = compare(r.tz, &r.samples);
    std::string fromTable;
    if (!r.name.empty() && inTable(r.name, &fromTable) && fromTable != r.tz) {
      r.tableTz = fromTable;  // the table of the library is older (or newer)
    }
    for (const ZoneEntry& e : zoneTable) {
      if (r.tz == e.tz && r.name != e.name) r.sameRules.push_back(e.name);
    }
  }
  return r;
}
