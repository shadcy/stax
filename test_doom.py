import pexpect
import sys

child = pexpect.spawn('make qemu-gfx', encoding='utf-8')
child.logfile = sys.stdout

child.expect('tios:\\/>', timeout=5)

child.sendline('mkdir TRASH')
child.expect('tios:\\/>', timeout=5)

child.sendline('cd TRASH')
child.expect('tios:\\/TRASH>', timeout=5)

child.sendline('doomgfx')
idx = child.expect(['DOOM.WAD is in the root directory. Do you want to go there\\? \\(y\\/n\\)>', pexpect.EOF, pexpect.TIMEOUT], timeout=5)
if idx == 0:
    child.sendline('n')
    child.expect('tios:\\/TRASH>', timeout=5)
    print("TEST PASSED")
else:
    print("TEST FAILED")

child.send('\x01x')
child.close()
