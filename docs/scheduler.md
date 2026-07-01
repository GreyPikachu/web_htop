# Optional scheduler latency profiler

`runqlat.bpf.c` is a separate CO-RE program. `main.cpp` loads it through libbpf and
reads cumulative per-CPU histograms as JSONL. It is not linked into the server and
does not alter its privilege requirements. These records are a separate profiler
format; the snapshot client's `--replay` accepts snapshot JSONL, not this format.

## Build and run

Requirements: Clang with the BPF backend, bpftool, libbpf development files,
pkg-config, a C++20 compiler, and readable `/sys/kernel/btf/vmlinux`. The build
script supports x86_64 and aarch64 and generates vmlinux.h from the local kernel.
The source accesses `task_struct.__state`, requiring a compatible modern kernel
BTF layout (Linux 5.14+ is the intended baseline).

```bash
bash scripts/build_bpf.sh
sudo ./build-bpf/web_htop_sched ./build-bpf/runqlat.bpf.o > scheduler.jsonl
```

Only the explicit profiler invocation needs BPF privileges. Privilege requirements
vary with kernel policy; the build itself does not use sudo.

## Measurement

Wakeup and new-task events record a TID's enqueue timestamp. A context switch also
records a task switched out while still runnable, including preemption. On switch-in,
the program looks up the TID, computes the delay and removes that timestamp. Using
TID rather than CPU preserves the association across migration. Task exit removes
an outstanding entry so it does not survive into a later TID lifetime.

The enqueue map is an LRU hash capped at 16,384 entries. The histogram is a per-CPU
array with 64 cumulative log2 buckets in microseconds. Bucket 0 includes 0 and 1 us;
bucket b>0 covers `[2^b, 2^(b+1))` us. Counters expose samples, switch-ins without
an observed enqueue, and map update failures.

An unmatched switch is not necessarily a lost event: the task may have been runnable
before attachment, its entry may have been evicted, or the observation may be absent
for another reason. LRU evictions are not individually counted. Per-CPU map reads
are not a globally atomic snapshot, so bucket sums and counters can differ slightly
while tracing continues. These are cumulative measurements, not exact percentiles.

## Validation gate

The profiler is experimental until verified on the target kernel. Its presence in
the repository is not evidence of a successful verifier/attach run. Before treating
its output as a performance result:

1. Build against the target kernel's BTF and inspect load/verifier diagnostics.
2. Compare idle and CPU-contended workloads; verify the latency distribution changes.
3. Exercise task migration, task exit and rapid task creation.
4. Check unmatched/update-failure counters and repeat with different concurrency.
5. Measure workload throughput and monitor CPU overhead with and without tracing.
6. Check detach on SIGINT/SIGTERM and failed partial attachment.

The normal monitor remains usable when any of these requirements is unavailable.
