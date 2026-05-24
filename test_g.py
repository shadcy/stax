import pexpect
import sys

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

child.expect('tios:\\/>', timeout=5)

child.sendline('help')
child.expect('tios:\\/>', timeout=5)

child.sendline('g:')
child.expect('tios:\\/>', timeout=5)

child.send('\x01x')
child.close()
