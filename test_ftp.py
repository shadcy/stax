import pexpect
import socket
import sys
import time

CTRL = ("127.0.0.1", 2121)


def recv_until(sock, marker, timeout=10):
    marker = marker.encode("ascii")
    sock.settimeout(timeout)
    data = b""
    while marker not in data:
        chunk = sock.recv(1024)
        if not chunk:
            break
        data += chunk
    return data.decode("ascii", "replace")


def recv_line(sock, timeout=10):
    return recv_until(sock, "\r\n", timeout)


def send_cmd(sock, command):
    sock.sendall(command.encode("ascii") + b"\r\n")
    time.sleep(0.1)


def main():
    build_out, build_status = pexpect.run("make", withexitstatus=True, encoding="utf-8")
    print(build_out)
    if build_status != 0:
        raise SystemExit(build_status)

    child = pexpect.spawn("make qemu", encoding="utf-8")
    child.logfile = sys.stdout

    try:
        child.expect("tios> Interactive command interface ready", timeout=15)
        child.expect(r"\[NET\] DHCP Success! IP: 10\.0\.2\.15", timeout=30)

        ctrl = socket.create_connection(CTRL, timeout=10)
        ctrl.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        print(recv_line(ctrl))
        send_cmd(ctrl, "USER anonymous")
        print(recv_line(ctrl))
        send_cmd(ctrl, "PASS anonymous")
        print(recv_line(ctrl))
        time.sleep(1.0)
        data_listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        data_listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        data_listener.bind(("0.0.0.0", 0))
        data_listener.listen(1)
        data_port = data_listener.getsockname()[1]
        send_cmd(ctrl, f"PORT 127,0,0,1,{data_port // 256},{data_port % 256}")
        print(recv_line(ctrl))
        send_cmd(ctrl, "STOR FTPTEST.TXT")
        data, _ = data_listener.accept()
        print(recv_line(ctrl))
        data.sendall(b"hello from linux via ftp\n")
        time.sleep(2.0)
        data.shutdown(socket.SHUT_WR)
        data.close()
        data_listener.close()
        print(recv_line(ctrl, timeout=20))

        send_cmd(ctrl, "QUIT")
        print(recv_line(ctrl))
        ctrl.close()

        child.expect("tios:", timeout=10)
        child.sendline("cat FTPTEST.TXT")
        child.expect("hello from linux via ftp", timeout=10)
        print("\n--- FTP TEST PASSED ---")
    finally:
        child.send("\x01\x18")


if __name__ == "__main__":
    main()
