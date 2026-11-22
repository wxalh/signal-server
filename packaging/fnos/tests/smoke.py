#!/usr/bin/env python3
"""Exercise the binaries and admin workflow from the finished fnOS archive."""

import argparse
import hashlib
import hmac
import http.client
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import tarfile
import tempfile
import time
import urllib.parse


PREFIX = "/app/signal-server/"
ADMIN = {"X-Trim-Isadmin": "true", "X-Trim-Userid": "1001"}
COOKIE = 0x2112A442


class UnixHTTP(http.client.HTTPConnection):
    def __init__(self, path):
        super().__init__("localhost", timeout=25)
        self.path = str(path)

    def connect(self):
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.settimeout(self.timeout)
        self.sock.connect(self.path)


def request(path, headers=None, form=None, endpoint=PREFIX):
    conn = UnixHTTP(path)
    headers = dict(headers or {})
    try:
        if form is None:
            conn.request("GET", endpoint, headers=headers)
        else:
            headers["Content-Type"] = "application/x-www-form-urlencoded"
            conn.request("POST", endpoint, urllib.parse.urlencode(form), headers)
        response = conn.getresponse()
        return response.status, response.read().decode("utf-8")
    finally:
        conn.close()


def csrf(page):
    return re.search(r'name="csrf"\s+value="([^"]+)"', page).group(1)


def check_nat_ports(page, signal_port, turn_port, relay_min, relay_max):
    table = re.search(r'<table id="nat-ports"[^>]*>(.*?)</table>', page, re.S)
    assert table is not None, "NAT mapping table is missing"
    rows = re.findall(r'<td>(.*?)</td>\s*<td><code>(.*?)</code></td>', table[1])
    scope = str(relay_min) if relay_min == relay_max else f"{relay_min}–{relay_max}"
    assert rows == [
        ("TCP", str(signal_port)), ("TCP / UDP", str(turn_port)),
        ("UDP", scope), ("TCP（按需）", scope),
    ], rows


def unused_port():
    with socket.socket() as tcp, socket.socket(type=socket.SOCK_DGRAM) as udp:
        tcp.bind(("0.0.0.0", 0))
        port = tcp.getsockname()[1]
        udp.bind(("0.0.0.0", port))
        return port


def check_ws(port, host="127.0.0.1"):
    with socket.create_connection((host, port), timeout=3) as conn:
        conn.sendall(
            b"GET /?sessionId=fnos-smoke&hostname=smoke HTTP/1.1\r\n"
            b"Host: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            b"Sec-WebSocket-Version: 13\r\n\r\n"
        )
        assert conn.makefile("rb").readline().startswith(b"HTTP/1.1 101 ")


def attr(kind, value):
    return struct.pack("!HH", kind, len(value)) + value + b"\0" * (-len(value) % 4)


def exchange(conn, kind, attrs=b"", key=None):
    tx = os.urandom(12)
    header = struct.pack("!HHI12s", kind, len(attrs) + (24 if key else 0), COOKIE, tx)
    outgoing = header + attrs
    if key:
        outgoing += attr(0x0008, hmac.digest(key, outgoing, "sha1"))
    conn.send(outgoing)
    result = conn.recv(4096)
    response, length, cookie, received = struct.unpack_from("!HHI12s", result)
    assert cookie == COOKIE and received == tx and len(result) == length + 20
    values = {}
    offset = 20
    while offset < len(result):
        tag, size = struct.unpack_from("!HH", result, offset)
        values[tag] = result[offset + 4 : offset + 4 + size]
        offset += 4 + ((size + 3) & ~3)
    return response, values


def check_turn(port, user=None, password=None, host="127.0.0.1", relay=(51000, 51099)):
    with socket.socket(type=socket.SOCK_DGRAM) as conn:
        conn.settimeout(4)
        conn.connect((host, port))
        assert exchange(conn, 0x0001)[0] == 0x0101
        if user is None:
            return
        transport = attr(0x0019, b"\x11\x00\x00\x00")
        kind, reply = exchange(conn, 0x0003, transport)
        assert kind == 0x0113 and reply[0x0009][2:4] == b"\x04\x01"
        realm, nonce = reply[0x0014], reply[0x0015]
        key = hashlib.md5(b":".join((user, realm, password)), usedforsecurity=False).digest()
        credentials = attr(0x0006, user) + attr(0x0014, realm) + attr(0x0015, nonce)
        kind, reply = exchange(conn, 0x0003, transport + credentials, key)
        assert kind == 0x0103 and 0x0016 in reply, "TURN did not allocate a relay address"
        relay_port = struct.unpack_from("!H", reply[0x0016], 2)[0] ^ (COOKIE >> 16)
        assert relay[0] <= relay_port <= relay[1]


def wait_ready(proc, sock):
    deadline = time.monotonic() + 25
    while time.monotonic() < deadline and proc.poll() is None:
        try:
            status, _ = request(sock, endpoint="/_health")
            if status == 200:
                return
        except (OSError, http.client.HTTPException):
            pass
        time.sleep(0.2)
    raise AssertionError("manager did not become ready")


def exercise(payload, work):
    state = work / "state"
    state.mkdir(mode=0o700)
    sock = work / "manager.sock"
    command = [str(payload / "bin/fnos-manager"), "--app-dir", str(payload),
               "--state-dir", str(state), "--socket", str(sock)]
    with (work / "manager.log").open("wb+") as log:
        proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            wait_ready(proc, sock)
            assert request(sock)[0] in (401, 403)
            assert request(sock, {**ADMIN, "X-Trim-Isadmin": "false"})[0] in (401, 403)
            status, page = request(sock, ADMIN)
            assert status == 200 and 'name="signal_port"' in page
            check_nat_ports(page, 3480, 3478, 49160, 49259)
            check_ws(3480)
            check_turn(3478)

            signal_port = unused_port()
            turn_port = unused_port()
            while 51000 <= signal_port <= 51099:
                signal_port = unused_port()
            while turn_port == signal_port or 51000 <= turn_port <= 51099:
                turn_port = unused_port()
            form = dict(action="save", csrf=csrf(page), signal_port=signal_port,
                        turn_port=turn_port, relay_min=51000, relay_max=51099,
                        realm="fnos-smoke.local", external_ip="", turn_user="test-webrtc",
                        turn_password="Smoke-Test-Only_12345")
            assert request(sock, ADMIN, {**form, "csrf": "invalid"})[0] in (400, 403)
            status, page = request(sock, ADMIN, form)
            assert status in (200, 303), (status, page[:150])
            check_ws(signal_port)
            check_turn(turn_port, form["turn_user"].encode(), form["turn_password"].encode())

            with socket.socket() as busy:
                busy.bind(("0.0.0.0", 0))
                busy.listen()
                status, page = request(sock, ADMIN)
                assert status == 200
                check_nat_ports(page, signal_port, turn_port, 51000, 51099)
                status, page = request(sock, ADMIN, {**form, "csrf": csrf(page),
                                                       "signal_port": busy.getsockname()[1]})
                assert status == 409
                check_nat_ports(page, signal_port, turn_port, 51000, 51099)
                check_ws(signal_port)
                check_turn(turn_port)

            proc.terminate()
            assert proc.wait(timeout=15) == 0
            proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
            wait_ready(proc, sock)
            status, page = request(sock, ADMIN)
            assert status == 200
            check_nat_ports(page, signal_port, turn_port, 51000, 51099)
            check_ws(signal_port)
            check_turn(turn_port, form["turn_user"].encode(), form["turn_password"].encode())
        except Exception:
            log.seek(0)
            print(log.read().decode(errors="replace"))
            raise
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="fnos-test-") as tmp:
        work = Path(tmp)
        payload = work / "app"
        payload.mkdir()
        with tarfile.open(args.package) as fpk:
            manifest = fpk.extractfile(next(m for m in fpk if Path(m.name).name == "manifest"))
            assert b"appname" in manifest.read()
            app = fpk.extractfile(next(m for m in fpk if Path(m.name).name == "app.tgz"))
            with tarfile.open(fileobj=app) as package:
                members = package.getmembers()
                for member in members:
                    target = payload / member.name
                    assert target.resolve().is_relative_to(payload) and (member.isfile() or member.isdir())
                package.extractall(payload, members=members)
        exercise(payload, work)
    print("fnOS package: settings authorization, NAT mapping, WebSocket, TURN allocation, port conflict and restart passed")


if __name__ == "__main__":
    main()
