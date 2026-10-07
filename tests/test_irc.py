#!/usr/bin/env python3
"""Functional tests for ircserv. Not part of the submission.

Usage:  python3 tests/test_irc.py [path/to/ircserv]
Starts its own server on a free port, runs every scenario and prints a
PASS/FAIL line for each check.
"""
import socket, subprocess, sys, time, os

BIN = sys.argv[1] if len(sys.argv) > 1 else "./ircserv"
PASSWORD = "secret"
FAILS = []
CHECKS = 0


def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p


PORT = free_port()


class C:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", PORT))
        self.buf = b""

    def send(self, line):
        self.s.sendall(line.encode() + b"\r\n")

    def raw(self, data):
        self.s.sendall(data)

    def read(self, wait=0.25):
        """Returns everything received during `wait` seconds, as lines."""
        self.s.settimeout(wait)
        end = time.time() + wait
        closed = False
        while time.time() < end:
            try:
                d = self.s.recv(65536)
                if not d:
                    closed = True
                    break
                self.buf += d
            except socket.timeout:
                break
            except (ConnectionResetError, BrokenPipeError):
                closed = True
                break
        lines = self.buf.decode(errors="replace").split("\r\n")
        self.buf = lines[-1].encode()
        self.closed = closed
        return lines[:-1]

    def reg(self, nick, password=PASSWORD):
        self.send("PASS " + password)
        self.send("NICK " + nick)
        self.send("USER %s 0 * :Real %s" % (nick, nick))
        return self.read()

    def close(self):
        self.s.close()


def check(name, cond, detail=""):
    global CHECKS
    CHECKS += 1
    print(("PASS  " if cond else "FAIL  ") + name + ("" if cond else "   <- " + repr(detail)))
    if not cond:
        FAILS.append(name)


def has(lines, *parts):
    return any(all(p in l for p in parts) for l in lines)


def main():
    srv = subprocess.Popen([BIN, str(PORT), PASSWORD], stdout=subprocess.DEVNULL)
    time.sleep(0.3)
    try:
        run(srv)
    finally:
        srv.terminate()
        code = srv.wait(timeout=5)
        check("server exits cleanly on SIGTERM", code == 0, code)
    print("\n%d checks, %d failed" % (CHECKS, len(FAILS)))
    sys.exit(1 if FAILS else 0)


def run(srv):
    # ---- registration ----
    a = C(); r = a.reg("alice")
    check("welcome 001", has(r, " 001 alice "), r)
    check("001-004 present", all(has(r, " %s alice" % n) for n in ("002", "003", "004")), r)

    x = C(); x.send("NICK bob"); x.send("USER bob 0 * :b"); r = x.read()
    check("no PASS -> 464 and closed", has(r, " 464 ") and has(r, "ERROR") and x.closed, r)

    x = C(); r = x.reg("bob", "wrong")
    check("wrong PASS -> 464 and closed", has(r, " 464 ") and x.closed, r)

    x = C(); x.send("JOIN #a"); r = x.read()
    check("command before registration -> 451", has(r, " 451 "), r)
    x.send("PASS " + PASSWORD); x.send("NICK alice"); r = x.read()
    check("nick in use -> 433", has(r, " 433 ", "alice"), r)
    x.send("NICK ALICE"); r = x.read()
    check("nick collision is case-insensitive", has(r, " 433 "), r)
    x.send("NICK 9bad"); r = x.read()
    check("invalid nick -> 432", has(r, " 432 "), r)
    x.send("NICK"); r = x.read()
    check("NICK without param -> 431", has(r, " 431 "), r)
    x.send("USER x"); r = x.read()
    check("USER missing params -> 461", has(r, " 461 "), r)
    x.send("USER bob 0 * :Bob"); x.send("NICK bob"); r = x.read()
    check("USER before NICK registers", has(r, " 001 bob "), r)
    x.send("PASS x"); x.send("USER a b c d"); r = x.read()
    check("re-register -> 462 twice", sum(" 462 " in l for l in r) == 2, r)
    b = x

    c = C(); c.send("CAP LS 302"); r = c.read()
    check("CAP LS answered", has(r, "CAP * LS"), r)
    r = c.reg("carol"); c.send("CAP END")
    check("registration after CAP", has(r, " 001 carol "), r)

    # ---- partial data (subject's nc + ctrl+D test) ----
    a.raw(b"PI"); time.sleep(0.1); a.raw(b"NG to"); time.sleep(0.1); a.raw(b"ken\r\n")
    r = a.read()
    check("command split in 3 packets", has(r, "PONG", "token"), r)
    a.raw(b"PING one\r\nPING two\r\nPING thr")
    r = a.read()
    check("two commands in one packet", has(r, ":one") and has(r, ":two") and not has(r, "thr"), r)
    a.raw(b"ee\n"); r = a.read()
    check("bare LF accepted", has(r, ":three"), r)
    a.send("FOO bar"); r = a.read()
    check("unknown command -> 421", has(r, " 421 ", "FOO"), r)
    a.raw(b"\r\n\r\n   \r\n"); a.send("ping x"); r = a.read()
    check("empty lines ignored, lowercase command ok", len(r) == 1 and has(r, "PONG"), r)

    # ---- channels ----
    a.send("JOIN #room"); r = a.read()
    check("JOIN echo", has(r, ":alice!alice@", "JOIN #room"), r)
    check("creator is operator in NAMES", has(r, " 353 ", "@alice") and has(r, " 366 "), r)
    b.send("JOIN #ROOM"); rb = b.read(); ra = a.read()
    check("channel names case-insensitive", has(rb, "JOIN #room") and has(rb, " 353 ", "bob"), rb)
    check("members see the JOIN", has(ra, ":bob!bob@", "JOIN #room"), ra)
    b.send("JOIN #room"); r = b.read()
    check("double JOIN is silent", r == [], r)
    a.send("JOIN nochan"); r = a.read()
    check("bad channel name -> 403", has(r, " 403 "), r)
    a.send("JOIN"); r = a.read()
    check("JOIN no params -> 461", has(r, " 461 "), r)

    # ---- messages ----
    a.send("PRIVMSG #room :hello all"); rb = b.read(); ra = a.read(); rc = c.read()
    check("channel message delivered", has(rb, ":alice!alice@", "PRIVMSG #room :hello all"), rb)
    check("not echoed to sender / non members", ra == [] and rc == [], (ra, rc))
    a.send("PRIVMSG bob :psst"); rb = b.read()
    check("private message", has(rb, ":alice!alice@", "PRIVMSG bob :psst"), rb)
    c.send("PRIVMSG #room :intruder"); r = c.read()
    check("non member cannot send -> 404", has(r, " 404 "), r)
    a.send("PRIVMSG nobody :x"); a.send("PRIVMSG #nope :x"); a.send("PRIVMSG"); a.send("PRIVMSG bob"); r = a.read()
    check("PRIVMSG errors 401 403 411 412", all(has(r, " %s " % n) for n in ("401", "403", "411", "412")), r)
    a.send("NOTICE nobody :x"); r = a.read()
    check("NOTICE never errors", r == [], r)
    a.send("PRIVMSG bob,carol :multi"); rb = b.read(); rc = c.read()
    check("multiple targets", has(rb, ":multi") and has(rc, ":multi"), (rb, rc))

    # ---- topic ----
    b.send("TOPIC #room"); r = b.read()
    check("no topic -> 331", has(r, " 331 "), r)
    b.send("TOPIC #room :by bob"); rb = b.read(); ra = a.read()
    check("anyone sets topic without +t", has(ra, ":bob!bob@", "TOPIC #room :by bob"), ra)
    a.send("MODE #room +t"); a.read(); b.read()
    b.send("TOPIC #room :denied"); r = b.read()
    check("+t blocks non operators -> 482", has(r, " 482 "), r)
    a.send("TOPIC #room :official"); a.read(); b.read()
    b.send("TOPIC #room"); r = b.read()
    check("topic readable -> 332", has(r, " 332 ", ":official"), r)
    c.send("TOPIC #room :x"); r = c.read()
    check("TOPIC by non member -> 442", has(r, " 442 "), r)

    # ---- modes ----
    b.send("MODE #room +i"); r = b.read()
    check("MODE by non operator -> 482", has(r, " 482 "), r)
    a.send("MODE #room"); r = a.read()
    check("mode query -> 324 +t", has(r, " 324 ", "#room +t"), r)
    a.send("MODE #room +ikl key1 2"); ra = a.read(); rb = b.read()
    check("combined +ikl with args", has(rb, "MODE #room +ikl key1 2"), rb)
    c.send("JOIN #room key1"); r = c.read()
    check("+i -> 473", has(r, " 473 "), r)
    a.send("MODE #room -i"); a.read(); b.read()
    c.send("JOIN #room"); r = c.read()
    check("+k without key -> 475", has(r, " 475 "), r)
    c.send("JOIN #room key1"); r = c.read()
    check("+l full -> 471", has(r, " 471 "), r)
    a.send("MODE #room +l 3"); a.read(); b.read()
    c.send("JOIN #room key1"); r = c.read(); a.read(); b.read()
    check("join with key and room", has(r, "JOIN #room") and has(r, " 332 ", "official"), r)
    a.send("MODE #room -kl+o key1 bob"); ra = a.read(); b.read(); c.read()
    check("-kl+o pairs args correctly", has(ra, "MODE #room -kl+o * bob"), ra)
    b.send("MODE #room -o alice"); ra = a.read(); b.read(); c.read()
    check("new operator can deop", has(ra, ":bob!bob@", "MODE #room -o alice"), ra)
    a.send("MODE #room +o alice"); r = a.read()
    check("deopped loses rights -> 482", has(r, " 482 "), r)
    b.send("MODE #room +o alice"); a.read(); b.read(); c.read()
    a.send("MODE #room +o ghost"); a.send("MODE #room +x"); a.send("MODE #room +k"); a.send("MODE #room +l abc"); r = a.read()
    check("mode errors 401 472 461 696", all(has(r, " %s " % n) for n in ("401", "472", "461", "696")), r)
    b.read(); c.read()
    a.send("MODE #room +t"); r = a.read()
    check("no-op mode change is silent", r == [], r)
    a.send("MODE #room b"); a.send("MODE alice +i"); a.send("WHO #room"); r = a.read()
    check("client noise: ban list, user mode, WHO", has(r, " 368 ") and has(r, " 315 ") and sum(" 352 " in l for l in r) == 3, r)

    # ---- kick / invite ----
    c.send("KICK #room bob"); r = c.read()
    check("KICK by non operator -> 482", has(r, " 482 "), r)
    a.send("KICK #room carol :bye bye"); ra = a.read(); rb = b.read(); rc = c.read()
    check("KICK seen by all incl. target", all(has(x, "KICK #room carol :bye bye") for x in (ra, rb, rc)), (ra, rb, rc))
    c.send("PRIVMSG #room :still here?"); r = c.read()
    check("kicked user is out", has(r, " 404 "), r)
    a.send("KICK #room carol"); a.send("KICK #nope x"); a.send("KICK #room"); r = a.read()
    check("KICK errors 441 403 461", all(has(r, " %s " % n) for n in ("441", "403", "461")), r)
    a.send("MODE #room +i"); a.read(); b.read()
    a.send("INVITE carol #room"); ra = a.read(); rc = c.read()
    check("INVITE -> 341 + notification", has(ra, " 341 ") and has(rc, ":alice!alice@", "INVITE carol :#room"), (ra, rc))
    c.send("JOIN #room"); r = c.read(); a.read(); b.read()
    check("invited user passes +i", has(r, "JOIN #room"), r)
    c.send("PART #room"); c.read(); a.read(); b.read()
    c.send("JOIN #room"); r = c.read()
    check("invitation is single use", has(r, " 473 "), r)
    a.send("INVITE bob #room"); a.send("INVITE ghost #room"); a.send("INVITE carol #nope"); r = a.read()
    check("INVITE errors 443 401 403", all(has(r, " %s " % n) for n in ("443", "401", "403")), r)
    c.send("INVITE alice #room"); r = c.read()
    check("INVITE by outsider -> 442", has(r, " 442 "), r)

    # ---- nick change / part / quit ----
    b.send("NICK bobby"); rb = b.read(); ra = a.read(); rc = c.read()
    check("NICK change broadcast to channel + self", has(ra, ":bob!bob@", "NICK :bobby") and has(rb, "NICK :bobby") and rc == [], (ra, rb, rc))
    a.send("NAMES #room"); r = a.read()
    check("NAMES reflects new nick", has(r, " 353 ", "bobby"), r)
    b.send("PART #room :see ya"); ra = a.read(); b.read()
    check("PART with reason", has(ra, ":bobby!bob@", "PART #room :see ya"), ra)
    b.send("PART #room"); b.send("PART #nope"); r = b.read()
    check("PART errors 442 403", has(r, " 442 ") and has(r, " 403 "), r)
    b.send("JOIN #room"); b.read()
    a.send("MODE #room -i"); a.read()
    b.send("JOIN #room"); b.read(); a.read()
    b.send("QUIT :gone"); rb = b.read(); ra = a.read()
    check("QUIT broadcast + ERROR + closed", has(ra, ":bobby!bob@", "QUIT :Quit: gone") and has(rb, "ERROR") and b.closed, (ra, rb))
    x = C(); r = x.reg("bobby")
    check("nick free after QUIT", has(r, " 001 bobby "), r)
    x.send("JOIN #room"); x.read(); a.read()
    x.close(); ra = a.read(0.5)
    check("abrupt disconnect broadcasts QUIT", has(ra, ":bobby!bobby@", "QUIT"), ra)

    # ---- channel lifetime ----
    a.send("PART #room"); a.read()
    a.send("JOIN #room"); r = a.read()
    check("empty channel destroyed, modes reset", has(r, "@alice"), r)
    a.send("MODE #room"); r = a.read()
    check("recreated channel has no modes", has(r, " 324 ", "#room +") and not has(r, "+t"), r)
    a.send("JOIN #x,#y,#z"); a.read(); a.send("JOIN 0"); r = a.read()
    check("JOIN 0 parts everything", sum(" PART " in l for l in r) == 4, r)

    # ---- suspended client: the flood must neither block the server nor be lost ----
    a.send("JOIN #flood"); a.read()
    z = C(); z.reg("zed"); z.send("JOIN #flood"); z.read(); a.read()
    payload = "x" * 400
    N = 15000                                   # ~6.5 MB, under the SendQ cap
    chunk = "".join("PRIVMSG #flood :%05d %s\r\n" % (i, payload) for i in range(N)).encode()
    a.raw(chunk)                                # zed is not reading meanwhile
    a.send("PING alive")
    r = []
    end = time.time() + 15
    while time.time() < end and not has(r, "PONG", "alive"):
        r += a.read(0.5)
    check("server responsive while a client is stalled", has(r, "PONG", "alive"), r[-3:])
    got = 0; last = ""
    end = time.time() + 20
    while time.time() < end and got < N:
        lines = z.read(0.5)
        got += sum("PRIVMSG #flood" in l for l in lines)
        if lines: last = lines[-1]
    check("stalled client receives all %d messages in order" % N, got == N and ("%05d" % (N - 1)) in last, (got, last[:60]))

    # ---- abuse ----
    y = C(); y.raw(b"A" * 10000); r = y.read(0.5)
    check("endless line without newline -> dropped", y.closed, r)
    socks = []
    for i in range(200):
        s = C(); s.send("PASS " + PASSWORD); s.send("NICK u%d" % i); s.send("USER u 0 * :u"); s.send("JOIN #crowd"); socks.append(s)
    time.sleep(1.0)
    a.send("JOIN #crowd"); r = []
    for _ in range(10):
        r += a.read(0.3)
    names = " ".join(l for l in r if " 353 " in l)
    check("200 simultaneous clients in one channel", names.count("u") >= 200, len(names))
    for s in socks:
        s.close()
    time.sleep(0.5); a.read(1.0)
    a.send("NAMES #crowd"); r = a.read()
    check("mass disconnect cleaned up", has(r, " 353 ", ":@alice") or has(r, " 353 ", ":alice"), r)
    y = C(); y.raw(b"\x00\xff\xfe garbage \x01\r\n:only-prefix\r\n:\r\n : : :\r\nMODE\r\nMODE #\r\nMODE :\r\nKICK , ,\r\nJOIN ,,,\r\nPRIVMSG , :x\r\n"); y.reg("yan")
    y.raw(b"MODE #crowd +++---\r\nMODE #crowd +ooo\r\nJOIN #,#,# ,,\r\nPART ,\r\nTOPIC\r\nINVITE\r\nWHO\r\nNAMES\r\nPRIVMSG :\r\nUSER\r\nPASS\r\nCAP\r\nPONG\r\nPING\r\n")
    y.read(0.5); a.read()
    a.send("PING final"); r = a.read()
    check("garbage input does not crash", has(r, "PONG", "final") and srv.poll() is None, r)


main()
