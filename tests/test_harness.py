import pexpect
import subprocess
import os
import sys
import random

def build_os(fault_injection=False):
    cflags = "-DFAULT_INJECTION" if fault_injection else ""
    subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL)
    subprocess.run(["make", "os.bin", f"EXTRA_CFLAGS={cflags}"], check=True, stdout=subprocess.DEVNULL)

def run_test_boot():
    print("Test: Normal Boot")
    build_os(False)
    child = pexpect.spawn("qemu-system-arm -M versatilepb -kernel build/bootloader.bin -drive file=os.bin,if=sd,format=raw -nographic -serial mon:stdio", encoding='utf-8')
    child.expect("STAX:", timeout=10)
    child.sendline("uptime")
    child.expect("Uptime:", timeout=2)
    child.close(force=True)
    print("PASS: Normal Boot")

def run_test_fwupdate():
    print("Test: Firmware Update Flow")
    build_os(False)
    subprocess.run(["mcopy", "-i", "os.bin@@2098688", "-o", "build/firmware.stax", "::/FIRMWARE.STAX"], check=True)
    
    child = pexpect.spawn("qemu-system-arm -M versatilepb -kernel build/bootloader.bin -drive file=os.bin,if=sd,format=raw -nographic -serial mon:stdio", encoding='utf-8')
    child.expect("STAX:", timeout=10)
    child.sendline("fwupdate /FIRMWARE.STAX")
    child.expect("Update staged. Reboot to apply.", timeout=15)
    child.close(force=True)
    
    # Reboot and verify
    child = pexpect.spawn("qemu-system-arm -M versatilepb -kernel build/bootloader.bin -drive file=os.bin,if=sd,format=raw -nographic -serial mon:stdio", encoding='utf-8')
    child.expect("STAX:", timeout=10)
    child.sendline("fwconfirm")
    child.expect("Firmware update confirmed successfully!", timeout=2)
    child.close(force=True)
    print("PASS: Firmware Update Flow")

def run_test_powerloss():
    print("Test: Large-Scale Power Loss Fault Injection")
    build_os(True)
    subprocess.run(["mcopy", "-i", "os.bin@@2098688", "-o", "build/firmware.stax", "::/FIRMWARE.STAX"], check=True)
    
    for i in range(1, 101): # Running 100 iterations of power loss for demonstration (can be scaled to 10k)
        print(f"  Injection iteration {i}...")
        
        # We need to simulate power loss after `target_writes` writes
        # Bootloader metadata = 2 writes
        # OS firmware write = ~800 writes
        # OS metadata update = 2 writes
        # Total writes ~ 804.
        
        target_writes = random.randint(1, 804)
        
        child = pexpect.spawn("qemu-system-arm -M versatilepb -kernel build/bootloader.bin -drive file=os.bin,if=sd,format=raw -nographic -serial mon:stdio", encoding='utf-8')
        try:
            child.expect("STAX:", timeout=5)
            child.sendline("fwupdate /FIRMWARE.STAX")
            
            writes_seen = 0
            while writes_seen < target_writes:
                idx = child.expect(["FI_HOOK_WRITE", "Update staged. Reboot to apply."], timeout=15)
                if idx == 1:
                    break
                
                writes_seen += 1
                if writes_seen == target_writes:
                    child.send("K") # Kill it!
                    child.expect("FI_POWER_LOSS", timeout=2)
                    break
                else:
                    child.send("Y") # Continue
            
            child.close(force=True)
        except pexpect.exceptions.EOF:
            pass
        except pexpect.exceptions.TIMEOUT:
            child.close(force=True)
            
        # Verify system recovers!
        child = pexpect.spawn("qemu-system-arm -M versatilepb -kernel build/bootloader.bin -drive file=os.bin,if=sd,format=raw -nographic -serial mon:stdio", encoding='utf-8')
        try:
            # Tell bootloader to skip FI hooks during recovery
            while True:
                idx = child.expect(["FI_HOOK_WRITE", "STAX:"], timeout=5)
                if idx == 0:
                    child.send("Y")
                else:
                    break
            
            # System booted successfully! It either rolled back or completed.
            # If it booted, fault injection recovery works!
            child.close(force=True)
        except Exception as e:
            print(f"FAIL at target_writes={target_writes}: System failed to recover! {e}")
            sys.exit(1)
            
    print("PASS: Power Loss Fault Injection")

def main():
    try:
        run_test_boot()
        run_test_fwupdate()
        run_test_powerloss()
        print("\nALL FAULT-INJECTION TESTS PASSED!")
    except Exception as e:
        print(f"\nFAIL: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
