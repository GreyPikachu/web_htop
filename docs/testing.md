# Tests and measurements

## Deterministic checks

`web_htop_test_core` uses explicit checks that remain active in Release builds.
It covers descriptor ownership, latest-wins queue transitions, every frame split
point, invalid frame lengths, JSON lifetimes and parser bounds, CPU regressions,
process comm parsing/PID reuse, cgroup identity changes, HTTP rejection rules and
concurrent immutable publication. The renderer is exercised without terminal I/O.

`tests/integration/test_runtime.py` starts a server with an isolated procfs fixture.
It checks stalled HTTP peers, a stream that stops reading, complete large frames,
half-closed clients, headless capture/replay, descriptor reclamation and SIGTERM
while sockets have pending work. A startup failure test verifies listener rollback.

```bash
ctest --test-dir build --output-on-failure --no-tests=error
```

## Sanitizers and fuzzing

Use separate build directories for ASan, UBSan and TSan. Run core and integration
under ASan/UBSan. TSan primarily targets the publication test and runtime shared
state; some container kernels prevent its shadow-memory mapping before main.
That is an environment failure, not a passing race check.

The Clang/libFuzzer targets exercise JSON plus domain decoding, fragmented frames
and Linux stat parsers. Libraries under test are compiled with coverage/sanitizer
instrumentation. For example:

```bash
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DWEB_HTOP_BUILD_FUZZERS=ON
cmake --build build-fuzz -j4
./build-fuzz/tests/web_htop_fuzz_json -max_total_time=60 -max_len=8192
```

The GitHub workflow defines GCC/Clang builds, sanitizers, retained GoogleTest suites
and short fuzz runs. A workflow definition is not evidence that a remote run passed.

## Benchmark

`scripts/benchmark.py` produces JSON containing configuration, request latencies,
server CPU time, RSS, payload size, collection duration and reactor counters. It
uses a synthetic procfs fixture and concurrent HTTP clients. Run a matrix of
process counts and client counts; keep raw reports with any performance claim.

```bash
python3 scripts/benchmark.py --server ./build/server/web_htop_server \
    --processes 1000 --clients 10 --requests 200 --output benchmark.json
```

This benchmark does not measure lossless delivery, allocator call counts, cold-cache
procfs behavior or multi-host networking. Streaming backpressure has dedicated
integration tests. Do not compare debug/sanitizer results with optimized builds.

For a real-host investigation, compare collection/encoding costs first. A flamegraph
or perf profile should precede specialized allocators, io_uring, RCU or sharding.
The design deliberately keeps those changes optional until a measured bottleneck
justifies them.
