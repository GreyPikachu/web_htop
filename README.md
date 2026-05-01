# web_htop

A Linux telemetry workstation: a C++20 sampling engine, a bounded HTTP/TCP reactor,
and a terminal client with live history, process exploration, recording and replay.

The server reads the selected procfs view, publishes immutable generations and
encodes each generation once. One I/O thread owns all sockets; a separate sampling
worker owns collector history. A subscriber that stops reading does not hold a
lock needed by another subscriber or by the collector.

## Build

Linux, a C++20 compiler (GCC 12+ with recent libstdc++, or Clang with an equivalent
standard library), CMake 3.20+, Python 3 for tests. GCC 13 is used for local validation.
The default build does not download dependencies.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWEB_HTOP_BUILD_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure --no-tests=error
```

CMake presets require CMake 3.21+. `cmake --preset asan` selects the AddressSanitizer
build; `tsan` selects ThreadSanitizer. Run these in separate build directories.
The retained GoogleTest codec suites are optional: install GoogleTest development
files and set `-DWEB_HTOP_BUILD_LEGACY_TESTS=ON`.

## Run

```bash
./build/server/web_htop_server
./build/client/web_htop_client localhost 9999 8080
```

The default bind address is `127.0.0.1`. Both ports are configurable; use `--help`
for the complete option list. Native Linux and WSL2 are supported environments.

The client provides Overview, Process Explorer, I/O, Pressure Transport and CPU Matrix
workspaces. Press `h` for keys. Use `c/m/p/t` to sort, `/` to filter, `j/k` to scroll,
and Space to freeze the visible generation while acquisition continues. A terminal
of at least 80 columns and 26 rows is needed for the full workspace; 120 x 40 is a
comfortable size. The palette is intentionally subdued.

```bash
# Capture received generations; the file is appended, never truncated.
./build/client/web_htop_client localhost 9999 8080 --record session.jsonl

# Inspect the same data without a running server (four generations per second).
./build/client/web_htop_client --replay session.jsonl

# Machine-readable output. A non-TTY client also emits JSONL.
./build/client/web_htop_client localhost 9999 8080 --once

# Observe a specific cgroup v2 directory in addition to procfs metrics.
./build/server/web_htop_server --cgroup /sys/fs/cgroup/my.slice
```

Remote access can use an SSH tunnel for both interfaces, while the server remains
bound to loopback:

```bash
ssh -N -L 9999:127.0.0.1:9999 -L 8080:127.0.0.1:8080 user@host
```

## Interfaces

| Endpoint | Meaning |
| --- | --- |
| `/health` | I/O loop is alive |
| `/ready` | CPU, memory and process collection are healthy and fresh |
| `/metrics` | Complete current generation, including provenance and optional sections |
| `/processes` | The transmitted top-K process set, with generation and truncation metadata |
| `/diagnostics` | Sessions, queue usage, dropped snapshots, timeouts and encoding cost |
| `/exporter` | Low-cardinality Prometheus metrics about the monitor itself |

TCP uses a four-byte big-endian length followed by JSON, protocol version 2.
Server and client from this revision must be upgraded together.

## Performance and failure behavior

The stream retains at most one in-flight frame and one pending frame per session.
A pending generation can be superseded; a partially written frame cannot. Logical
queued bytes across all sessions are capped at 64 MiB. These are delivery limits,
not a bound on the process collector's working set or the kernel's socket memory.

Measure the actual workload before making throughput claims:

```bash
python3 scripts/benchmark.py --server ./build/server/web_htop_server \
    --processes 1000 --clients 10 --requests 200 --output benchmark.json
```

The benchmark records its environment and raw measurements. Integration tests
separately cover stalled readers, partial requests, disconnects and shutdown.

## Optional scheduler profiler

`tools/scheduler` contains a separate libbpf/CO-RE program for system-wide run-queue
latency histograms. It is opt-in, requires kernel BTF and BPF privileges, and is not
loaded by the ordinary server. See [scheduler profiler](docs/scheduler.md) for
build instructions, measurement semantics and the kernel validation gate.

## Documentation

- [Applying the standalone refactor](docs/apply_refactor.md)
- [Executed validation](docs/validation.md)
- [Build and operations](docs/build_run.md)
- [Architecture and ownership](docs/architecture.md)
- [Metric definitions](docs/metrics.md)
- [HTTP API](docs/http_api.md)
- [Streaming protocol](docs/protocol.md)
- [Tests and measurements](docs/testing.md)
- [Design decisions](docs/decisions.md)
- [Refactor coverage and limitations](docs/refactor_notes.md)
