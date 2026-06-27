# Design decisions

## C++20 as the baseline

The runtime uses jthread/stop_token, stop-aware condition-variable waits, atomic
shared pointers, span, ranges and constrained numeric helpers. These replace manual
lifetime/cancellation scaffolding. C++23 is not required merely to increase the
language version; distribution toolchains with mature C++20 support are sufficient.

## Level-triggered epoll

The workload is latest-state telemetry with moderate fan-out. Level-triggered epoll
allows explicit fairness budgets without maintaining a second ready queue to resume
edge-triggered work. It removes blocking session I/O and thread-per-client growth.
The client uses poll because it owns only two sockets and terminal input.

## Immutable publication before custom lock-free structures

Reader lifetime is the hard part of replacing a mutex-protected snapshot. Shared
ownership supplies a clear reclamation rule. The actual atomic implementation can
use a lock internally. Diagnostics expose this; benchmarks decide whether a more
specialized scheme is worth its maintenance cost.

## Latest wins

A dashboard needs a recent complete generation. Delivering a long backlog increases
latency while making old information look live. The queue keeps one in-flight frame
and one replaceable pending frame. This policy is unsuitable for lossless audit events;
such events would need their own durable contract, not a larger snapshot queue.

## Own JSON strings

A serializable value should not silently depend on the lifetime of the object used
to create it. Owning strings removes that dependency and makes queued/cached
representations safe. The parser no longer maintains a second permanent string list.
Serialization happens once per generation, outside the I/O loop.

## Restricted HTTP surface

GET-only, close-after-response HTTP is enough for the current read-only API. Body
processing and keep-alive would enlarge the parser/state space without enabling a
current feature. The supported subset and explicit rejection rules are documented.
A future richer HTTP API should use a maintained protocol library rather than grow
an incidental parser indefinitely.

## No automatic eBPF privilege escalation

The scheduler profiler is a separate binary and optional build. BPF capability,
verifier and kernel-BTF failures cannot bring down the monitor. The ordinary server
never invokes sudo, loads programs or silently changes system configuration.
