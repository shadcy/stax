import pexpect
import sys
import time
import telnetlib

child = pexpect.spawn('bash', ['-c', 'make clean && make qemu-gfx'], encoding='utf-8')
child.logfile = sys.stdout

# Wait for the prompt
try:
    child.expect(r'tios.*>', timeout=60)
    print("\n\n=== PROMPT APPEARED ===\n")
    # Wait a bit longer to allow loops to print
    time.sleep(2)

    tn = telnetlib.Telnet("127.0.0.1", 1234)
    tn.read_until(b"(qemu) ")

    for key in ['p', 'i', 'n', 'g', 'spc', '1', '0', 'dot', '0', 'dot', '2', 'dot', '2']:
        tn.write(("sendkey %s\n" % key).encode())
        tn.read_until(b"(qemu) ")
        time.sleep(0.05)

    tn.write(b"sendkey ret\n")
    tn.read_until(b"(qemu) ")

    time.sleep(5)

except Exception as e:
    print(e)

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
