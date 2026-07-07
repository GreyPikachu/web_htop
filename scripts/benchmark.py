#!/usr/bin/env python3
"""Run an isolated telemetry workload; save measurements, not a pass/fail speed claim."""

import argparse
import concurrent.futures
import importlib.util
import json
import os
from pathlib import Path
import platform
import statistics
import tempfile
import time

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "runtime", root / "tests/integration/test_runtime.py"
)
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--server", default=str(root / "build/server/web_htop_server"))
    p.add_argument("--processes", type=int, default=1000)
    p.add_argument("--clients", type=int, default=10)
    p.add_argument("--requests", type=int, default=200)
    p.add_argument("--output", default="benchmark.json")
    args = p.parse_args()
    if (
        not 1 <= args.processes <= 10000
        or not 1 <= args.clients <= 256
        or not 10 <= args.requests <= 100000
    ):
        p.error("processes: 1..10000, clients: 1..256, requests: 10..100000")
    with tempfile.TemporaryDirectory() as temp:
        proc = Path(temp) / "proc"
        runtime.fixture(proc, args.processes)
        with runtime.server(
            args.server,
            "--proc-root",
            str(proc),
            "--sys-root",
            temp,
            "--max-processes",
            str(args.processes),
            "--interval-ms",
            "100",
        ) as (server, http, stream):
            time.sleep(0.5)
            start = json.loads(runtime.get(http, "/diagnostics")[1])
            begin = time.perf_counter()

            def request(_):
                t = time.perf_counter()
                status, body = runtime.get(http, "/metrics")
                if status != 200:
                    raise RuntimeError(f"HTTP {status}")
                data = json.loads(body)
                return (
                    (time.perf_counter() - t) * 1000,
                    len(body),
                    data["telemetry"]["collection_us"],
                )

            with concurrent.futures.ThreadPoolExecutor(
                max_workers=args.clients
            ) as pool:
                samples = list(pool.map(request, range(args.requests)))
            elapsed = time.perf_counter() - begin
            end = json.loads(runtime.get(http, "/diagnostics")[1])
            durations = sorted(item[0] for item in samples)
            percentile = lambda q: durations[
                min(len(durations) - 1, int((len(durations) - 1) * q))
            ]
            report = {
                "platform": platform.platform(),
                "cpu_count": os.cpu_count(),
                "server": str(Path(args.server).resolve()),
                "fixture_processes": args.processes,
                "concurrent_http_clients": args.clients,
                "requests": args.requests,
                "wall_seconds": elapsed,
                "requests_per_second": args.requests / elapsed,
                "latency_ms": {
                    "p50": percentile(0.5),
                    "p95": percentile(0.95),
                    "p99": percentile(0.99),
                    "max": max(durations),
                },
                "payload_bytes": samples[-1][1],
                "collection_us_median": statistics.median(s[2] for s in samples),
                "server_cpu_seconds": (end["process_cpu_us"] - start["process_cpu_us"])
                / 1000000,
                "server_rss_peak_bytes": end["rss_peak_bytes"],
                "server_fds": end["open_fds"],
                "diagnostics": json.loads(runtime.get(http, "/diagnostics")[1]),
                "limitations": "Synthetic procfs, warm filesystem cache, client/server share one machine. HTTP load only; streaming isolation is tested separately.",
            }
            Path(args.output).write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
