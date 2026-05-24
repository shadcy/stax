import pexpect
import sys

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

child.expect('tios:\\/>', timeout=5)

child.sendline('mkdir MYDIR')
child.expect('tios:\\/>', timeout=5)

child.sendline('cd MYDIR')
child.expect('tios:\\/MYDIR>', timeout=5)

child.sendline('cd ..')
child.expect('tios:\\/>', timeout=5)

child.send('\x01x')
child.close()
