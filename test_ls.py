import pexpect
import sys

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

child.expect('tios:\\/>', timeout=5)

child.sendline('ls')
child.expect('tios:\\/>', timeout=5)

child.sendline('ls --size')
child.expect('tios:\\/>', timeout=5)

child.send('\x01x')
child.close()
