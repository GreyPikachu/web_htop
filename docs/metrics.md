# Metric definitions

| Field / section | Definition |
| --- | --- |
| `timestamp` | Wall-clock milliseconds since the Unix epoch; presentation only |
| `telemetry.interval_ms` | Requested sampling interval |
| `collection_us` | Monotonic duration of the collection pipeline |
| CPU total/per-core | Busy fraction from deltas of user, nice, system, idle, iowait, irq, softirq, steal |
| Process CPU% | `delta(utime + stime) / CLK_TCK / elapsed_seconds * 100`; one busy core is 100% |
| Process identity | `(pid, starttime_ticks)` within the reported boot |
| RSS | `/proc/PID/stat` RSS pages multiplied by `_SC_PAGESIZE`; Linux's approximate resident accounting |
| Memory used | `MemTotal - MemAvailable` |
| Filesystem used | `(f_blocks - f_bfree) * f_frsize`; available uses `f_bavail` |
| Interface rates | Per-interface byte deltas divided by monotonic elapsed seconds |
| `rx_kbps`, `tx_kbps` | Legacy field names; their unit is **KiB/s**, not kilobits/s |
| Disk throughput | Sector deltas multiplied by 512, divided by elapsed seconds |
| Disk IOPS | Read completions/s + write completions/s |
| Disk busy% | Delta of I/O-active milliseconds divided by elapsed milliseconds, capped at 100% |
| PSI some | Percentage of time at least one task was stalled on the resource |
| PSI full | Percentage of time all non-idle tasks were stalled on the resource |
| Cgroup CPU% | Delta of `usage_usec` divided by elapsed seconds and 10,000; one core is 100% |
| Cgroup throttled ms/s | Delta of `throttled_usec` divided by elapsed seconds and 1,000 |

CPU guest/guest_nice fields are not added again: they are already accounted for in
user/nice. A decreasing CPU counter invalidates that interval, including a decreasing
iowait counter. CPU identities are actual CPU IDs rather than vector positions.

Rates require two observations of the same entity. A first sample, a counter reset,
a missing previous sample or a nonpositive interval yields no rate. New interface
identities use ifindex where sysfs makes it available; falling back to a name cannot
detect every remove/recreate event between samples. These cases are observable
sampling limitations, not grounds to manufacture a rate.

Process stat is read once per PID, including comm, starttime, RSS and thread count.
There is no three-file join across process lifetimes. The parser handles spaces,
closing parentheses and newlines in comm. PID enumeration and stat reading can still
race process exit; such exits are counted separately from permissions and malformed
records. A PID reused with a different starttime gets a fresh CPU baseline.

`max_processes` limits the transmitted top-K set, ordered by CPU, RSS and PID.
`total_processes` counts successfully sampled processes before truncation. Client
sorting/filtering applies to the transmitted set; it is not a query over every
process on the machine. Permission failures mean that process totals are incomplete.

## Validity

Collector status is `ok`, `warming_up`, `partial` or `unavailable`. Existing numeric
fields remain for API continuity and can contain zero in an unavailable section;
consumers must inspect status. New per-interface/per-device rates use JSON null for
an unavailable interval. Per-process `cpu_valid` controls whether CPU% is meaningful.

The memory collector requires MemAvailable rather than guessing a replacement.
Filesystem reserved blocks are neither free-for-user nor used blocks, so used plus
available need not equal total. Disk busy% is not a universal saturation indicator,
particularly for parallel and stacked devices. Device rows are never summed.

The legacy CPU frequency field is retained at zero; this revision does not sample
frequency. Core usage, CPU IDs, load and pressure are the supported CPU diagnostics.

## Cgroup scope

Cgroup collection is opt-in through an explicit v2 directory. Procfs and cgroup
metrics remain separate. `memory_unlimited` and `cpu_unlimited` distinguish a `max`
limit from an absent controller. A missing directory or identity change invalidates
the previous CPU baseline. Not all controllers expose all fields at the root.
PSI is optional; lack of kernel support does not stop ordinary telemetry.

## Sources

- [Linux procfs documentation](https://docs.kernel.org/filesystems/proc.html)
- [proc_stat(5)](https://man7.org/linux/man-pages/man5/proc_stat.5.html)
- [proc_pid_stat(5)](https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html)
- [Cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [PSI](https://docs.kernel.org/accounting/psi.html)
- [Disk statistics](https://docs.kernel.org/admin-guide/iostats.html)
