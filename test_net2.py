import pexpect
import sys
import time
import telnetlib

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

# Wait for the prompt
child.expect(r'tios.*>', timeout=30)
print("\n\n=== PROMPT APPEARED ===\n")

# Wait a bit longer to allow DHCP to finish (if it can)
time.sleep(5)

# Connect to QEMU monitor
tn = telnetlib.Telnet("127.0.0.1", 1234)
tn.read_until(b"(qemu) ")

tn.write(b"sendkey i\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey f\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey c\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey o\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey n\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey f\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey i\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey g\n")
tn.read_until(b"(qemu) ")
tn.write(b"sendkey ret\n")
tn.read_until(b"(qemu) ")

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

child.send('\x01x')
child.close()
