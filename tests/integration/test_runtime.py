#!/usr/bin/env python3
"""Black-box socket/lifecycle tests. Uses loopback and temporary /proc fixtures."""

import argparse
import concurrent.futures
import contextlib
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import tempfile
import time


def ports():
    with contextlib.ExitStack() as stack:
        sockets = [stack.enter_context(socket.socket()) for _ in range(2)]
        for s in sockets:
            s.bind(("127.0.0.1", 0))
        return [s.getsockname()[1] for s in sockets]


def get(port, path="/health"):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    try:
        connection.request("GET", path)
        response = connection.getresponse()
        return response.status, response.read()
    finally:
        connection.close()


def exact(s, size):
    data = bytearray()
    while len(data) < size:
        part = s.recv(size - len(data))
        if not part:
            raise AssertionError("truncated stream")
        data.extend(part)
    return data


def frame(s):
    (size,) = struct.unpack("!I", exact(s, 4))
    assert 0 < size <= 8 * 1024 * 1024
    return json.loads(exact(s, size))


def fixture(root, count=1000):
    root.mkdir(parents=True, exist_ok=True)
    (root / "stat").write_text("cpu 100 0 0 100 0 0 0 0\ncpu0 100 0 0 100 0 0 0 0\n")
    (root / "meminfo").write_text("MemTotal: 1000000 kB\nMemAvailable: 500000 kB\n")
    (root / "loadavg").write_text("1 2 3 1/100 1\n")
    (root / "net").mkdir(exist_ok=True)
    (root / "net/dev").write_text("eth0: 100 0 0 0 0 0 0 0 200 0 0 0 0 0 0 0\n")
    (root / "diskstats").write_text("8 0 sda 10 0 100 0 10 0 100 0 0 10 0\n")
    for pid in range(1, count + 1):
        folder = root / str(pid)
        folder.mkdir(exist_ok=True)
        (folder / "stat").write_text(
            f"{pid} (worker_{pid:05d}) R 1 1 1 0 0 0 0 0 0 0 100 0 0 0 20 0 2 0 {pid} 100000 64\n"
        )


@contextlib.contextmanager
def server(binary, *extra):
    http, stream = ports()
    process = subprocess.Popen(
        [binary, "--http-port", str(http), "--stream-port", str(stream), *extra],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError(process.communicate())
            try:
                if get(http)[0] == 200:
                    break
            except OSError:
                time.sleep(0.03)
        else:
            raise AssertionError("server startup timeout")
        yield process, http, stream
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
        try:
            out, err = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            out, err = process.communicate()
            raise AssertionError(f"shutdown hung\n{out}\n{err}")
        if process.returncode != 0:
            raise AssertionError(f"server exited {process.returncode}\n{out}\n{err}")


def run(args):
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp) / "proc"
        fixture(root)
        with server(
            args.server,
            "--proc-root",
            str(root),
            "--sys-root",
            temp,
            "--interval-ms",
            "100",
            "--write-timeout-ms",
            "500",
            "--request-timeout-ms",
            "500",
        ) as (process, http, stream):
            held = []
            try:
                for _ in range(24):
                    s = socket.create_connection(("127.0.0.1", http))
                    s.sendall(b"GET / HTTP/1.1\r\nHost:")
                    held.append(s)
                with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
                    assert all(
                        status == 200
                        for status, _ in pool.map(lambda _: get(http), range(32))
                    )
                print("PASS slow HTTP clients do not block health")
                with socket.create_connection(("127.0.0.1", stream), timeout=3) as fast:
                    fast.shutdown(socket.SHUT_WR)
                    first = frame(fast)
                    second = frame(fast)
                    assert first["protocol_version"] == 2
                    assert (
                        second["telemetry"]["sequence"] > first["telemetry"]["sequence"]
                    )
                    assert len(second["process"]["processes"]) == 1000
                print("PASS complete large frames and half-closed client")
                slow = socket.socket()
                slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
                slow.connect(("127.0.0.1", stream))
                held.append(slow)
                with socket.create_connection(("127.0.0.1", stream), timeout=3) as fast:
                    seq = 0
                    for _ in range(15):
                        current = frame(fast)["telemetry"]["sequence"]
                        assert current > seq
                        seq = current
                diagnostics = json.loads(get(http, "/diagnostics")[1])
                assert diagnostics["timeouts_total"] >= 1
                assert diagnostics["queued_bytes"] <= diagnostics["queue_budget_bytes"]
                print("PASS slow subscriber isolation and deadlines")
                status, body = get(http, "/metrics")
                assert (
                    status == 200
                    and len(json.loads(body)["process"]["processes"]) == 1000
                )
                capture = Path(temp) / "record.jsonl"
                result = subprocess.run(
                    [
                        args.client,
                        "127.0.0.1",
                        str(stream),
                        str(http),
                        "--once",
                        "--record",
                        str(capture),
                    ],
                    capture_output=True,
                    text=True,
                    timeout=6,
                )
                assert result.returncode == 0, result.stderr
                recorded = json.loads(result.stdout)
                assert recorded["protocol_version"] == 2
                assert capture.read_text() == result.stdout
                replay = subprocess.run(
                    [args.client, "--replay", str(capture), "--once"],
                    capture_output=True,
                    text=True,
                    timeout=3,
                )
                assert replay.returncode == 0, replay.stderr
                assert (
                    json.loads(replay.stdout)["telemetry"]["sequence"]
                    == recorded["telemetry"]["sequence"]
                )
                print("PASS client live receive and recording replay")
                for s in held:
                    s.close()
                held.clear()
                time.sleep(0.7)
                before = json.loads(get(http, "/diagnostics")[1])["open_fds"]
                for _ in range(100):
                    s = socket.create_connection(("127.0.0.1", stream))
                    s.setsockopt(
                        socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0)
                    )
                    s.close()
                time.sleep(0.8)
                after = json.loads(get(http, "/diagnostics")[1])["open_fds"]
                assert after <= before + 2, (before, after)
                print("PASS reset/disconnect descriptor reclamation")
            finally:
                for s in held:
                    s.close()
            # Leave both a partial HTTP request and a stalled stream during SIGTERM.
            with socket.create_connection(
                ("127.0.0.1", http)
            ) as partial, socket.create_connection(("127.0.0.1", stream)):
                partial.sendall(b"GET /")
                start = time.monotonic()
                process.send_signal(signal.SIGTERM)
                process.wait(timeout=2)
                assert time.monotonic() - start < 2
            print("PASS shutdown during pending I/O")
        http, stream = ports()
        with socket.socket() as occupied:
            occupied.bind(("127.0.0.1", stream))
            occupied.listen()
            failed = subprocess.run(
                [args.server, "--http-port", str(http), "--stream-port", str(stream)],
                capture_output=True,
                timeout=3,
            )
            assert failed.returncode != 0
            with socket.socket() as check:
                check.bind(("127.0.0.1", http))
        print("PASS failed startup rolls back the first listener")
        bad = subprocess.run(
            [args.server, "--http-port", "70000"], capture_output=True, timeout=3
        )
        assert bad.returncode != 0
        print("PASS strict configuration validation")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    run(parser.parse_args())
