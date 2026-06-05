import pexpect
import sys
import time

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

# Wait for the prompt
try:
    child.expect(r'tios.*>', timeout=15)
    print("\n\n=== PROMPT APPEARED ===\n")
    # Wait a bit longer to allow packets to print
    time.sleep(3)
except Exception as e:
    print(e)

child.send('\x01x')
child.close()
