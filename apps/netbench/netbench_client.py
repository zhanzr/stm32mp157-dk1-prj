#!/usr/bin/env python3
"""netbench client: measure TCP throughput against the netbench server on the board.

usage:
  netbench_client.py upload   <host> [port] [MiB]
  netbench_client.py download <host> [port] [MiB]
"""
import socket
import sys
import time

DEFAULT_PORT = 5201
DEFAULT_MIB = 100


def upload(host, port, mib):
    n = mib * 1048576
    s = socket.create_connection((host, port))
    s.sendall(b"W")
    chunk = bytes(1048576)
    sent = 0
    t0 = time.perf_counter()
    while sent < n:
        sent += s.send(chunk[: min(len(chunk), n - sent)])
    s.shutdown(socket.SHUT_WR)
    dt = time.perf_counter() - t0
    print(f"client {host}: SENT {mib} MiB in {dt:.2f} s = "
          f"{n * 8 / dt / 1e6:.1f} Mbps ({n / dt / 1048576:.1f} MiB/s)")
    print("  server says:", s.recv(200).decode(errors="replace").strip())
    s.close()


def download(host, port, mib):
    n = mib * 1048576
    s = socket.create_connection((host, port))
    s.sendall(b"R" + str(n).encode() + b"\n")
    got = 0
    t0 = time.perf_counter()
    while True:
        d = s.recv(1048576)
        if not d:
            break
        got += len(d)
    dt = time.perf_counter() - t0
    print(f"client {host}: RECV {got} bytes in {dt:.2f} s = "
          f"{got * 8 / dt / 1e6:.1f} Mbps ({got / dt / 1048576:.1f} MiB/s)")
    s.close()


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "upload"
    host = sys.argv[2] if len(sys.argv) > 2 else "192.168.5.165"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_PORT
    mib = int(sys.argv[4]) if len(sys.argv) > 4 else DEFAULT_MIB
    if mode == "upload":
        upload(host, port, mib)
    elif mode == "download":
        download(host, port, mib)
    else:
        sys.exit(__doc__)
