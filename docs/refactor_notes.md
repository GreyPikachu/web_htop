# Refactor coverage

| Proposed issue | Implementation |
| --- | --- |
| Descriptor ownership | UniqueFd, socket ownership confined to reactor |
| Slow TCP subscribers | Nonblocking writes, latest-wins queue, global budget and no-progress deadline |
| HTTP concurrency | Epoll sessions, bounded reads/writes, absolute header deadline |
| Startup/shutdown | Transactional listener construction, signalfd, jthread and stop-aware wait |
| Counter correctness | Monotonic rates, reset handling, CPU IDs and explicit units |
| Process identity | PID + starttime, single-stat sample, separate error counts |
| Immutable state | Atomic shared ownership and one encoding per generation |
| Testable collectors | LinuxSource injection, pure parsers, section quality |
| Protocol/reconnect | Version 2, size/depth limits, instance/sequence, jittered reconnect |
| JSON ownership | Owning DOM strings, bounded parsing, typed boundary validation |
| Verification | Core and socket tests, sanitizer configurations, fuzz targets, CI |
| Performance | Reproducible benchmark with configuration and raw JSON results |
| Operations | Loopback default, strict options, systemd example, terminal sanitization |
| cgroup/PSI | Opt-in cgroup v2 and optional procfs/cgroup PSI sections |
| Scheduler analysis | Separate experimental CO-RE/libbpf profiler; target-kernel validation required |

## Compatibility

Run server and client from the same revision. The stream payload is version 2;
HTTP metric names are retained with extra telemetry metadata. Numeric legacy fields
need their associated validity status. Internal scaffold APIs have been replaced;
there is no ABI compatibility promise for the static libraries.

Old empty runtime placeholders were removed rather than maintained next to the new
runtime. Existing metric models and their codec tests were retained. Original author
attribution in existing files is preserved; new files document responsibilities and
invariants without inventing an author or a historical date.

## Scope that remains explicit

There is no TLS/authentication server, process-kill endpoint, lossless history server,
remote command execution or automatic deployment. Remote use is supported through
SSH tunneling. The optional eBPF tool is not loaded by the server and does not yet
feed a terminal workspace. Kernel-dependent verification is required before making
claims about its results.

The UI records received full snapshots, not all server generations. Backpressure
can skip generations. The process collector still scans the selected procfs view;
top-K reduces sorting/transmission, not the fundamental cost of enumeration.
