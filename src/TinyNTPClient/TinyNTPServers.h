/**
 * @file TinyNTPServers.h
 * @brief Defines for well known public NTP servers.
 *
 * Usage: TinyNTPClient<WiFiUDP> ntp(NTP_IP_GOOGLE_1);
 *
 * - NTP_IP_*   IP addresses: they can be used without DNS. All addresses
 *              were checked with an NTP request on 2026-10-31. Only the
 *              operators of the servers can guarantee that they stay the same:
 *              use the NTP_HOST_* names if your stack has a DNS resolver.
 * - NTP_HOST_* host names: always resolve to current servers
 *
 * Please respect the usage policies of the operators and do not poll more
 * often than necessary (e.g. once per hour is plenty).
 */

#pragma once

// --- Host names -------------------------------------------------------------
#define NTP_HOST_POOL "pool.ntp.org"
#define NTP_HOST_POOL_EUROPE "europe.pool.ntp.org"
#define NTP_HOST_POOL_NORTH_AMERICA "north-america.pool.ntp.org"
#define NTP_HOST_POOL_SOUTH_AMERICA "south-america.pool.ntp.org"
#define NTP_HOST_POOL_ASIA "asia.pool.ntp.org"
#define NTP_HOST_POOL_OCEANIA "oceania.pool.ntp.org"
#define NTP_HOST_POOL_AFRICA "africa.pool.ntp.org"
#define NTP_HOST_POOL_CH "ch.pool.ntp.org"
#define NTP_HOST_POOL_DE "de.pool.ntp.org"
#define NTP_HOST_POOL_US "us.pool.ntp.org"
#define NTP_HOST_GOOGLE "time.google.com"
#define NTP_HOST_CLOUDFLARE "time.cloudflare.com"
#define NTP_HOST_NIST "time.nist.gov"
#define NTP_HOST_FACEBOOK "time.facebook.com"
#define NTP_HOST_APPLE "time.apple.com"
#define NTP_HOST_MICROSOFT "time.windows.com"
#define NTP_HOST_PTB "ptbtime1.ptb.de"  // Germany
#define NTP_HOST_METAS "ntp.metas.ch"   // Switzerland

// --- IP addresses -----------------------------------------------------------
// Google Public NTP (time.google.com), leap seconds are smeared
#define NTP_IP_GOOGLE_1 "216.239.35.0"
#define NTP_IP_GOOGLE_2 "216.239.35.4"
#define NTP_IP_GOOGLE_3 "216.239.35.8"
#define NTP_IP_GOOGLE_4 "216.239.35.12"

// Cloudflare (time.cloudflare.com)
#define NTP_IP_CLOUDFLARE_1 "162.159.200.1"
#define NTP_IP_CLOUDFLARE_2 "162.159.200.123"

// NIST, USA (time.nist.gov is load balanced, time-?-g.nist.gov are single servers)
#define NTP_IP_NIST "132.163.97.1"
#define NTP_IP_NIST_A "129.6.15.28"
#define NTP_IP_NIST_B "129.6.15.29"
#define NTP_IP_NIST_C "129.6.15.30"

// Meta (time.facebook.com), leap seconds are smeared
#define NTP_IP_FACEBOOK "129.134.26.123"

// PTB, Germany (ptbtime1/2/3.ptb.de)
#define NTP_IP_PTB_1 "192.53.103.108"
#define NTP_IP_PTB_2 "192.53.103.104"
#define NTP_IP_PTB_3 "192.53.103.103"

// METAS, Switzerland (ntp.metas.ch)
#define NTP_IP_METAS "195.176.26.215"

// There are no IP addresses for pool.ntp.org, time.apple.com and
// time.windows.com: they are different for each region and change often. Use
// the NTP_HOST_* names for them.
