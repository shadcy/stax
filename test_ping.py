import pexpect
import sys
import time

def main():
    print("Compiling...")
    pexpect.run("make clean")
    pexpect.run("make")
    
    print("Starting QEMU...")
    child = pexpect.spawn("make qemu", encoding='utf-8')
    child.logfile = sys.stdout
    
    try:
        child.expect("tios> Interactive command interface ready", timeout=10)
        print("\n\n--- Prompt detected, waiting for DHCP... ---")
        
        def type_slowly(cmd):
            for char in cmd:
                child.send(char)
                time.sleep(0.05)
            child.send('\r')
            time.sleep(0.2)

        time.sleep(2)
        
        # Send ifconfig
        type_slowly("ifconfig")
        time.sleep(1)
        
        # Send ping
        type_slowly("ping 8.8.8.8")
        
        # Dump everything until EOF or Timeout
        try:
            print(child.read())
        except pexpect.TIMEOUT:
            print("Finished reading")
            
        print("\n\n--- DONE ---")
        
    except pexpect.TIMEOUT:
        print("\n\n--- TIMEOUT ---")
    except pexpect.EOF:
        print("\n\n--- EOF ---")
    finally:
        child.send('\x01\x18') # Ctrl-A X

if __name__ == "__main__":
    main()
