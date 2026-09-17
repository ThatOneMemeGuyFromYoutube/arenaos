#!/usr/bin/env python3
"""Drive the NT31 binary in a pty: boot, open Console, run VER, exit."""
import os, pty, select, sys, time, signal, re, fcntl, struct, termios

BIN = sys.argv[1] if len(sys.argv) > 1 else "./nt31"

def sgr_click(x, y):
    return ("\x1b[<0;%d;%dM" % (x + 1, y + 1) +
            "\x1b[<0;%d;%dm" % (x + 1, y + 1))

master, slave = pty.openpty()
fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))

pid = os.fork()
if pid == 0:
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
    os.dup2(slave, 0)
    os.dup2(slave, 1)
    os.dup2(slave, 2)
    os.close(master)
    os.execv(BIN, [BIN, "--fast"])
os.close(slave)

out = b""
T0 = None
def pump(dur, marker=None):
    global out, T0
    if T0 is None: T0 = time.monotonic()
    end = time.time() + dur
    while time.time() < end:
        r, _, _ = select.select([master], [], [], 0.02)
        if r:
            try:
                d = os.read(master, 65536)
            except OSError:
                return False
            if not d:
                return False
            out += d
            if marker and marker in d:
                print("driver: saw '%s' at +%.3f" % (marker, time.monotonic()-T0))
                marker = None
    return True

t0 = time.monotonic()
pump(2.5)                      # boot + splash
# keyboard path: Program group is focused by default
os.write(master, b"\x1b[C")   # RIGHT -> select "Console"
pump(0.8)
os.write(master, b"\r")       # ENTER -> open it
pump(1.2)
os.write(master, b"VER")
pump(0.8)
os.write(master, b"\r")
pump(1.2)
os.write(master, b"\x03")           # Ctrl-C -> confirm exit
pump(0.6)
os.write(master, b"y")
pump(0.6)
os.write(master, b"\r")             # confirm shutdown screen
pump(1.5)

try:
    os.kill(pid, signal.SIGKILL)
except ProcessLookupError:
    pass
try:
    os.waitpid(pid, 0)
except ChildProcessError:
    pass
os.close(master)

open("test/raw.log","wb").write(out)
text = out.decode("utf-8", "replace")
clean = re.sub(r"\x1b\[[0-9;?<>]*[A-Za-z]", "", text)
clean = re.sub(r"\x1b[O]", "", clean)
lines = [l for l in clean.splitlines() if l.strip()]

print("=== last 40 non-empty lines ===")
for l in lines[-40:]:
    print(repr(l))

checks = {
    "boot banner": "Microsoft(R) Windows NT(TM) 3.1" in clean,
    "starting nt": "Starting Windows NT..." in clean,
    "program manager": "Program Manager" in clean,
    "groups": all(g in clean for g in ("Program", "Favorites", "Startup", "Windows Setup")),
    "console prompt": "C:\\>" in clean,
    "ver output": "Windows NT [Version 3.10.103]" in clean,
    "exit confirm": "Are you sure you want to exit Windows NT?" in clean,
    "shutdown": "Windows NT is shutting down" in clean,
    "safe to turn off": "safe to turn off" in clean,
}
print("=== checks ===")
ok = True
for k, v in checks.items():
    print(("PASS " if v else "FAIL "), k)
    ok = ok and v
sys.exit(0 if ok else 1)
