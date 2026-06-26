# Architecture

## Ownership

The server has two threads in steady state. The foreground thread owns the reactor,
listeners, all accepted sockets, session queues and network counters. The sampling
`std::jthread` owns the collectors and their previous observations. It never calls
`send`, routes an HTTP request, or touches a session.

`LinuxSource` is the external-data boundary. Production reads Linux files;
deterministic tests supply the same interface with fixtures. Text parsing and
counter arithmetic are independent functions. A collector failure becomes a
section status; other sections still run. Whole-generation serialization failure
leaves the previous published generation intact and eventually fails readiness.

## Publication

A generation is fully assembled, encoded, framed and timestamped before publication.
`SharedState` release-stores a `shared_ptr<const PublishedSnapshot>`; readers use
an acquire-load and retain the generation they selected. Both the object and its
encoded strings are immutable after publication.

Acquire/release establishes visibility of the initialized object. `shared_ptr`
provides reclamation while a reader still owns an old generation. It does not make
the publication primitive lock-free: the implementation reports `is_lock_free()`
in diagnostics. A custom reclamation scheme would need a measured reason and a
separate correctness argument.

A generation is an application-level publication boundary, not a transaction
against the kernel. CPU, memory and process data are read sequentially. Collection
start/end times and per-section durations expose the resulting sampling window.
The selected `/proc` mount can reflect a container's namespace. The scope label is
`procfs-view`; it deliberately does not claim host-wide visibility.

## Reactor

The reactor uses level-triggered epoll. Each turn limits accepts to 32 and each
session write to 64 KiB. HTTP reads are bounded by the 8 KiB header limit. Readiness
remains registered when a budget is exhausted; the next turn continues the work.

Epoll event data contains a monotonically increasing session token, not a raw FD.
An event left in a returned batch cannot accidentally address a new session after
the kernel reuses a descriptor number. Only the reactor closes network descriptors.
`UniqueFd` handles ownership transfer and every failure path.

An `eventfd` makes publication visible to the I/O loop. Its counter is a wakeup,
not a generation queue. Several notifications can collapse into one; the reactor
loads the latest generation. A 100 ms timerfd drives deadline checks and also
provides a recovery poll for publication notifications.

A full HTTP response is selected from one generation. One request is served per
connection; there is no keep-alive or request-body support. HTTP and TCP share the
same readiness loop but have separate session states.

## Backpressure

The current frame has an immutable byte buffer and a send offset. Until any byte
is sent, a newer frame can replace it. Once sending starts, a second slot holds the
latest pending frame. Replacing a pending frame increments the superseded counter.
Finishing the current frame promotes the pending frame.

The invariant is that the bytes of a frame are never interleaved with a newer
frame. Each client either receives a complete generation or observes a disconnect;
sequence numbers tell it which complete generations it skipped.

The global 64 MiB limit sums remaining bytes per session, even when a buffer is
shared. This deliberately conservative accounting can reject a connection before
physical user-space memory reaches that size. It does not account for kernel
buffers, allocator overhead or collector working memory. The process response
limit bounds transmission, not how many `/proc` entries must be inspected.

A write deadline measures time without progress. It is not reset by queuing a new
generation. Header deadlines are absolute from accept, so sending one byte at a
time cannot keep an HTTP session alive indefinitely.

## Lifecycle

The foreground thread blocks SIGINT/SIGTERM before creating the worker and accepts
them through signalfd. There is no asynchronous server handler executing C++ code.
Both listeners must bind successfully before the collector starts. RAII rolls back
partial startup.

On shutdown, the reactor releases sessions, then the worker receives a stop request.
Its timed wait is stop-aware, and the process scan checks cancellation between PIDs.
Sockets cannot hold shutdown in a blocking send/recv. A kernel filesystem operation
such as statvfs can still block inside the kernel; cancellation is cooperative and
cannot promise a hard deadline for an unresponsive filesystem. systemd has a final
service-level timeout for that case.

## Client

The terminal owns one poll loop. Streaming and diagnostic HTTP connections are
nonblocking, each with their own state and deadline. The UI refreshes independently
of received generations. DNS resolution happens once before terminal mode is entered;
its timeout follows libc resolver configuration.

Received generations are decoded and optionally recorded. Freeze holds the visible
generation, not the socket reader. Replay reads JSONL at four generations per second;
it preserves stored sequence numbers and timestamps, not original wall-clock timing.

The renderer uses a bounded character canvas. Remote strings are converted to a
printable ASCII presentation before being placed in cells. ANSI control sequences
are emitted only by the renderer. Terminal settings, alternate screen and cursor
visibility are restored on normal exit and SIGINT/SIGTERM.
