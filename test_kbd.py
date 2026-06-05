import pexpect
import sys
import time
import telnetlib

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

# Wait for the prompt
child.expect(r'tios.*>', timeout=15)
print("\n\n=== PROMPT APPEARED ===\n")

# Connect to QEMU monitor
time.sleep(1)
tn = telnetlib.Telnet("127.0.0.1", 1234)
tn.read_until(b"(qemu) ")

# Send keys with pauses
for key in ['h', 'e', 'l', 'p']:
    time.sleep(0.5)
    tn.write(("sendkey %s\n" % key).encode())
    tn.read_until(b"(qemu) ")
    time.sleep(0.3)

# Send enter
time.sleep(0.5)
tn.write(b"sendkey ret\n")
tn.read_until(b"(qemu) ")

# Wait for output
time.sleep(3)

# Grab whatever appeared
remaining = child.before if child.before else ""
try:
    child.expect(pexpect.TIMEOUT, timeout=2)
except:
    pass
remaining += child.before if child.before else ""

print("\n\n=== OUTPUT ===")
print(remaining)
print("=== END ===\n")

# Quit
child.send('\x01x')
child.close()
