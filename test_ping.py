import pexpect
import sys
import time

PROMPT = "tios:"

def main():
    print("Building...")
    build_out, build_status = pexpect.run("make", withexitstatus=True, encoding="utf-8")
    print(build_out)
    if build_status != 0:
        raise SystemExit(build_status)
    
    print("Starting QEMU...")
    child = pexpect.spawn("make qemu", encoding='utf-8')
    child.logfile = sys.stdout
    failures = []
    
    try:
        child.expect("tios> Interactive command interface ready", timeout=10)
        print("\n\n--- Prompt detected, waiting for DHCP... ---")

        def type_slowly(cmd):
            for char in cmd:
                child.send(char)
                time.sleep(0.02)
            child.send('\r')

        def wait_prompt(timeout=30):
            child.expect(PROMPT, timeout=timeout)

        def run_check(command, expected, timeout=45, required=True, cancel_after_success=False):
            print(f"\n\n--- {command} ---")
            wait_prompt()
            type_slowly(command)
            try:
                child.expect(expected, timeout=timeout)
                print(f"\n[PASS] saw: {expected}")
                if cancel_after_success:
                    child.send('\x1b')
                return True
            except pexpect.TIMEOUT:
                msg = f"{command}: did not see {expected!r}"
                print(f"\n[FAIL] {msg}")
                child.send('\x1b')
                if required:
                    failures.append(msg)
                return False

        # Let DHCP complete and prove address, gateway, and DNS configuration.
        child.expect(r"\[NET\] DHCP Success! IP: 10\.0\.2\.15", timeout=30)
        run_check("ifconfig", "DNS Server : 10.0.2.3", timeout=15)

        # Local slirp endpoints prove L2/ARP/IP path inside QEMU user networking.
        run_check("ping 10.0.2.2", "Ping reply received!", timeout=45, required=False)
        run_check("ping 10.0.2.3", "Ping reply received!", timeout=45, required=False)

        # Public DNS resolution plus outbound TCP/HTTP prove internet access even
        # on hosts where QEMU user-mode ICMP echo is filtered or unsupported.
        run_check("ping google.com", "Pinging target IP", timeout=45, cancel_after_success=True)
        run_check("fetch 1.1.1.1", "Connected! Sending GET request...", timeout=60)

        if failures:
            print("\n\n--- FAILURES ---")
            for failure in failures:
                print(failure)
            raise SystemExit(1)

        print("\n\n--- NETWORK TESTS PASSED ---")

    except pexpect.TIMEOUT:
        print("\n\n--- TIMEOUT ---")
        raise SystemExit(1)
    except pexpect.EOF:
        print("\n\n--- EOF ---")
        raise SystemExit(1)
    finally:
        child.send('\x01\x18') # Ctrl-A X

if __name__ == "__main__":
    main()
