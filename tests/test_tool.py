#!/usr/bin/env python3
"""Tests of the command line tool `tinyntp` together with a device that runs
a TinyNTPServer (test_device). The messages that set the time are implemented
here a second time with python's hashlib/hmac, independent of the library.

Usage: test_tool.py <tinyntp> <test_device> [test name ...]
(with test names only these tests are run, e.g. test_zone_lookup)"""
import hashlib, hmac, os, re, socket, struct, subprocess, sys, time

TOOL, DEVICE = sys.argv[1], sys.argv[2]
BASE = 30000 + os.getpid() % 20000
failed = 0
checks = 0


def check(name, ok, detail=""):
    global failed, checks
    checks += 1
    if not ok:
        failed += 1
    print(("[ OK ] " if ok else "[FAIL] ") + name + ("" if ok else "  " + str(detail)))


def run(*args, env=None, timeout=20):
    e = dict(os.environ)
    e.pop("TINYNTP_KEY", None)
    for k, v in (env or {}).items():
        if v is None:
            e.pop(k, None)   # None: the variable must not be set
        else:
            e[k] = v
    p = subprocess.run([TOOL, *args], capture_output=True, text=True, env=e, timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def device(port, mode, seconds):
    p = subprocess.Popen([DEVICE, str(port), mode, str(seconds)], stdout=subprocess.PIPE, text=True)
    time.sleep(0.4)
    return p


def now_ms():
    return int(time.time() * 1000)


def ntp_time_ms(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)
    req = bytearray(48)
    req[0] = 0x1B
    s.sendto(bytes(req), ("127.0.0.1", port))
    d = s.recvfrom(100)[0]
    sec = struct.unpack(">I", d[40:44])[0] - 2208988800
    frac = struct.unpack(">I", d[44:48])[0]
    return sec * 1000 + (frac * 1000 >> 32)


# ---- the tool as client ------------------------------------------------------
def test_help_and_options():
    rc, out, err = run("-h")
    check("-h shows the usage", rc == 0 and "Usage:" in out and "tinyntp set" in out)
    rc, out, err = run("--bogus")
    check("unknown option -> exit 2", rc == 2 and "unknown option" in err)
    rc, out, err = run("-p")
    check("missing value -> exit 2", rc == 2 and "missing value" in err)
    rc, out, err = run("-p", "70000")
    check("invalid port -> exit 2", rc == 2)
    rc, out, err = run("-z", "garbage", "127.0.0.1")
    check("invalid time zone -> exit 2", rc == 2 and "unknown time zone" in err)
    rc, out, err = run("-z", "argentina", "127.0.0.1")
    check("time zone that is not unique -> exit 2", rc == 2 and "not unique" in err)


def test_query_a_server():
    port = BASE
    p = device(port, "clock", 6)
    # set the time of the device first, to have a valid time for the queries
    rc, out, err = run("set", "127.0.0.1", "-p", str(port), "-k", "topsecret")
    check("set: exit 0", rc == 0, (out, err))
    rc, out, err = run("127.0.0.1", "-p", str(port), "-z", "CET-1CEST,M3.5.0,M10.5.0/3")
    check("query: exit 0", rc == 0, err)
    m = re.search(r"UTC:\s+(\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)\.(\d{3})", out)
    check("query: shows the UTC time", m is not None, out)
    if m:
        t = time.gmtime(time.time())
        check("query: UTC date is today", (int(m[1]), int(m[2]), int(m[3])) == (t.tm_year, t.tm_mon, t.tm_mday))
    check("query: shows the local time and the name", re.search(r"local:\s+.* (CET|CEST)", out) is not None, out)
    off = re.search(r"clock offset:\s+([+-]\d+\.\d+) s", out)
    check("query: clock offset is small", off is not None and abs(float(off[1])) < 0.1, out)
    rc, out, err = run("127.0.0.1", "-p", str(port), "-z", "JST-9")
    check("query: time zone without summer time", "JST" in out and "summer time" not in out, out)
    for z in ("Europe/Zurich", "zurich", "EUROPE/ZURICH"):
        rc, out, err = run("127.0.0.1", "-p", str(port), "-z", z)
        check("query: -z with the name %s" % z, rc == 0 and re.search(r"local:\s+.* (CET|CEST)", out) is not None, (rc, out, err))
    rc, out, err = run("127.0.0.1", "-p", str(port), "-z", "Asia/Kolkata")
    check("query: -z Asia/Kolkata shows IST", rc == 0 and " IST" in out, out)
    rc, out, err = run("127.0.0.1", "-p", str(port), "-z", "local", env={"TZ": "Asia/Tokyo"} if HAVE_ZONEINFO else {"TZ": "JST-9"})
    check("query: -z local uses the time zone of this computer", rc == 0 and " JST" in out, (rc, out, err))
    p.wait()


def test_query_failures():
    rc, out, err = run("127.0.0.1", "-p", str(BASE + 1), "-t", "400")
    check("no server -> exit 1", rc == 1 and "no response" in err, (rc, err))


# ---- set the time with the tool ----------------------------------------------
def test_set_with_the_tool():
    port = BASE + 2
    p = device(port, "clock", 5)
    before = ntp_time_ms(port) if False else None
    rc, out, err = run("set", "127.0.0.1", "-p", str(port), "-k", "topsecret")
    check("set: exit 0", rc == 0, (out, err))
    check("set: shows the new time and the round trip", "time set:" in out and "round trip:" in out, out)
    check("device has the right time now", abs(ntp_time_ms(port) - now_ms()) < 100)
    rc, out, err = run("set", "127.0.0.1", "-p", str(port), env={"TINYNTP_KEY": "topsecret"})
    check("set: the secret from TINYNTP_KEY", rc == 0, (out, err))
    rc, out, err = run("set", "127.0.0.1", "-p", str(port), "-k", "wrong")
    check("set: wrong secret -> exit 3", rc == 3 and "wrong secret" in err, (rc, err))
    rc, out, err = run("set", "127.0.0.1", "-p", str(port))
    check("set: no secret -> exit 2", rc == 2 and "secret" in err, (rc, err))
    rc, out, err = run("set", "-k", "x")
    check("set: no device -> exit 2", rc == 2 and "address of the device" in err, (rc, err))
    p.wait()
    check("device counted the set requests", "timeset=2" in p.stdout.read())


def test_set_failures():
    port = BASE + 3
    p = device(port, "nosetting", 3)
    rc, out, err = run("set", "127.0.0.1", "-p", str(port), "-k", "topsecret", "-t", "500")
    check("time setting is off -> exit 1", rc == 1 and "no response" in err, (rc, err))
    p.wait()
    port = BASE + 4
    p = device(port, "fixed", 3)
    rc, out, err = run("set", "127.0.0.1", "-p", str(port), "-k", "topsecret")
    check("source cannot be set -> exit 3", rc == 3 and "cannot be set" in err, (rc, err))
    p.wait()


# ---- the tool as server -------------------------------------------------------
def test_serve():
    port = BASE + 5
    p = subprocess.Popen([TOOL, "serve", "-p", str(port), "-s", "2"], stdout=subprocess.PIPE, text=True)
    time.sleep(0.4)
    t = ntp_time_ms(port)
    check("serve: serves the time of this computer", abs(t - now_ms()) < 50)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)
    req = bytearray(48)
    req[0] = 0x1B
    s.sendto(bytes(req), ("127.0.0.1", port))
    d = s.recvfrom(100)[0]
    check("serve: stratum from -s", d[1] == 2)
    rc, out, err = run("127.0.0.1", "-p", str(port))
    check("serve: the tool can query its own server", rc == 0 and "UTC:" in out, (out, err))
    p.send_signal(2)  # ctrl-c
    out = p.communicate(timeout=5)[0]
    check("serve: prints the number of requests at the end", re.search(r"served \d+ requests", out) is not None, out)
    rc, out, err = run("serve", "-p", "1")
    check("serve: port without permission -> exit 1", rc == 1 and "cannot listen" in err or os.geteuid() == 0, (rc, err))


# ---- the protocol, implemented independently in python -----------------------
# ---- time zones: look up a zone, find the zone of this computer ---------------
def table_from_header():
    """The zones of src/TinyNTP/TinyTimeZones.h: {IANA name: POSIX string}"""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "TinyNTP", "TinyTimeZones.h")
    zones = {}
    for line in open(path):
        m = re.match(r'#define TZ_[A-Z0-9_]+\s+"([^"]*)"\s+// (\S+)', line)
        if m:
            zones[m[2]] = m[1]
    return zones


def test_zone_lookup():
    rc, out, err = run("tz", "Europe/Zurich")
    check("tz <name>: exit 0 and the string", rc == 0 and "CET-1CEST,M3.5.0,M10.5.0/3" in out and "zone:        Europe/Zurich" in out, (rc, out, err))
    check("tz <name>: shows the time in the zone", re.search(r"now:\s+\d{4}-\d\d-\d\d \d\d:\d\d:\d\d (CET|CEST)", out) is not None, out)
    check("tz <name>: shows zones with the same rules", "same rules:" in out and "Europe/Berlin" in out, out)
    for text, expected in (("zurich", "Europe/Zurich"), ("EUROPE/zurich", "Europe/Zurich"),
                           ("new york", "America/New_York"), ("Kolkata", "Asia/Kolkata"),
                           ("Buenos_Aires", "America/Argentina/Buenos_Aires"),
                           # a city that is a part of other names: the city wins
                           ("Bahia", "America/Bahia"), ("dawson", "America/Dawson"), ("omsk", "Asia/Omsk"),
                           ("Etc/GMT", "Etc/GMT")):
        rc, out, err = run("tz", text)
        check("tz %r finds %s" % (text, expected), rc == 0 and ("zone:        " + expected) in out, (rc, out, err))
    rc, out, err = run("tz", "Asia/Kolkata")
    check("tz Asia/Kolkata: IST-5:30", "IST-5:30" in out)
    rc, out, err = run("tz", "nowhere")
    check("tz <unknown> -> exit 1", rc == 1 and "unknown time zone" in err, (rc, err))
    rc, out, err = run("tz", "argentina")
    check("tz <part of several names> lists them, exit 1", rc == 1 and out.count("America/Argentina/") >= 10 and "match" in err, (rc, err))
    rc, out, err = run("tz", "-l")
    lines = [l for l in out.splitlines() if l.strip()]
    check("tz -l lists 461 zones", rc == 0 and len(lines) == 461, len(lines))
    rc, out, err = run("tz", "-l", "kolkata")
    check("tz -l <text> filters", rc == 0 and out.split() == ["Asia/Kolkata", "IST-5:30"], out)
    # the table of the tool is the same as the defines of the library
    listed = {l.split()[0]: l.split()[1] for l in lines}
    header = table_from_header()
    check("the table of the tool equals the TZ_* defines (%d zones)" % len(header), listed == header,
          (set(listed.items()) ^ set(header.items())))


ZONEINFO = ("/usr/share/zoneinfo", "/usr/lib/zoneinfo", "/var/db/timezone/zoneinfo")
HAVE_ZONEINFO = any(os.path.exists(os.path.join(d, "Europe/Zurich")) for d in ZONEINFO)


def test_local_zone():
    if HAVE_ZONEINFO:
        for name, tz in (("Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"), ("America/New_York", "EST5EDT,M3.2.0,M11.1.0"),
                         ("Asia/Kolkata", "IST-5:30"), ("Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"),
                         ("Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"), ("UTC", "UTC0"),
                         (":Europe/London", "GMT0BST,M3.5.0/1,M10.5.0")):
            rc, out, err = run("tz", env={"TZ": name})
            shown = name.lstrip(":")
            check("tz with TZ=%s: zone and string" % name, rc == 0 and ("zone:        " + shown) in out and tz in out, (rc, out, err))
            check("tz with TZ=%s: the string is read from the zoneinfo file" % name, "from the zoneinfo file" in out, out)
            check("tz with TZ=%s: the check passes" % name, "same local time as this computer" in out, out)
    else:
        print("[SKIP] no zoneinfo files on this computer: the zone names are not tested")
    rc, out, err = run("tz", env={"TZ": "CET-1CEST,M3.5.0,M10.5.0/3"})
    check("tz with a POSIX string in TZ", rc == 0 and "zone:        unknown" in out and "from the TZ variable" in out and "CET-1CEST,M3.5.0,M10.5.0/3" in out, (rc, out, err))
    rc, out, err = run("tz", env={"TZ": "XYZ-3:30XYZS,M2.2.0,M9.1.0/4"})
    check("tz with a POSIX string that is not in the table", rc == 0 and "XYZ-3:30XYZS,M2.2.0,M9.1.0/4" in out and "same rules" not in out, (rc, out, err))
    rc, out, err = run("tz", env={"TZ": "Nowhere/Land"})
    check("tz with a name that nobody knows: found by comparing with the table",
          rc == 0 and "zone:        unknown" in out and "comparing the local time with the table" in out and "same local time" in out, (rc, out, err))
    rc, out, err = run("tz", env={"TZ": None})
    check("tz without TZ: the zone of this computer", rc == 0 and "TZ string:" in out and "same local time" in out, (rc, out, err))


KEY = b"topsecret"


def sock():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)
    return s


def recv(s):
    try:
        return s.recvfrom(100)[0]
    except socket.timeout:
        return None


def challenge(s, addr):
    s.sendto(bytes([0x27, 1]), addr)
    d = recv(s)
    assert d and d[0] == 0x27 and d[1] == 2 and len(d) == 10, d
    return d[2:10]


def set_request(nonce, ms, key=KEY, flip=None):
    body = bytes([3]) + nonce + struct.pack(">Q", ms)
    msg = bytearray(bytes([0x27]) + body + hmac.new(key, body, hashlib.sha256).digest())
    if flip is not None:
        msg[flip] ^= 1
    return bytes(msg)


def status(d):
    return None if d is None else d[2]


def test_protocol_attacks():
    port = BASE + 6
    addr = ("127.0.0.1", port)
    p = device(port, "clock", 30)
    s = sock()
    check("device starts in 2021", ntp_time_ms(port) < 1700000000000)
    n = challenge(s, addr)
    check("two challenges give different nonces", challenge(s, addr) != n)
    n = challenge(s, addr)
    s.sendto(set_request(n, now_ms(), key=b"nope"), addr)
    check("wrong secret -> rejected", status(recv(s)) == 1)
    for idx, name in [(12, "time"), (3, "nonce"), (40, "signature")]:
        s.sendto(set_request(n, now_ms(), flip=idx), addr)
        check("tampered %s -> rejected" % name, status(recv(s)) == 1)
    check("nothing changed so far", ntp_time_ms(port) < 1700000000000)
    good = set_request(n, now_ms())
    s.sendto(good, addr)
    check("valid request after failed attempts -> ok", status(recv(s)) == 0)
    check("device has the right time", abs(ntp_time_ms(port) - now_ms()) < 50)
    s.sendto(good, addr)
    check("REPLAY of the valid request -> rejected", status(recv(s)) == 1)
    s.sendto(set_request(n, now_ms()), addr)
    check("used nonce with a new signature -> rejected", status(recv(s)) == 1)
    n2 = challenge(s, addr)
    s.sendto(set_request(n2, 1000000000000), addr)
    check("time before 2020 -> invalid time", status(recv(s)) == 3)
    for junk in (bytes([0x27]), bytes([0x27, 9, 1, 2]), os.urandom(7), bytes([0x27, 3]) + os.urandom(20)):
        s.sendto(junk, addr)
        recv(s)
    check("device still answers NTP after junk", abs(ntp_time_ms(port) - now_ms()) < 50)
    n3 = challenge(s, addr)
    time.sleep(9.0)
    s.sendto(set_request(n3, now_ms()), addr)
    check("nonce is valid after 9 s", status(recv(s)) == 0)
    n4 = challenge(s, addr)
    time.sleep(10.6)
    s.sendto(set_request(n4, now_ms()), addr)
    check("nonce is expired after 10.6 s", status(recv(s)) == 1)
    n5 = challenge(s, addr)
    s.sendto(set_request(n5, now_ms()), addr)
    check("a fresh nonce works again", status(recv(s)) == 0)
    p.terminate()


TESTS = (test_help_and_options, test_zone_lookup, test_local_zone, test_query_a_server, test_query_failures,
         test_set_with_the_tool, test_set_failures, test_serve, test_protocol_attacks)
for t in TESTS:
    if len(sys.argv) > 3 and t.__name__ not in sys.argv[3:]:
        continue
    print("--", t.__name__)
    t()
print("tool: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
