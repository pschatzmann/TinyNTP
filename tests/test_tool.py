#!/usr/bin/env python3
"""Tests of the command line tool `tinyntp` together with a device that runs
a TinyNTPServer (test_device). The messages that set the time are implemented
here a second time with python's hashlib/hmac, independent of the library.

Usage: test_tool.py <tinyntp> <test_device>"""
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
    e.update(env or {})
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
    check("invalid time zone -> exit 2", rc == 2 and "invalid time zone" in err)


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


for t in (test_help_and_options, test_query_a_server, test_query_failures,
          test_set_with_the_tool, test_set_failures, test_serve, test_protocol_attacks):
    print("--", t.__name__)
    t()
print("tool: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
