# Module boundaries

This revision replaces the earlier branch scaffold with a working runtime.

- `common`: metric values, owned JSON documents, wire codec, descriptor ownership.
- `server/system`: procfs/sysfs/cgroup/filesystem access boundary.
- `server/collectors`: pure sample parsing, delta arithmetic and the sampling pipeline.
- `server/state`: immutable generation publication and pre-encoded representations.
- `server/transport`: one epoll owner and bounded session output queues.
- `server/http`: documented GET-only protocol subset.
- `server/app`: startup and cancellation order.
- `client/net`: stream and diagnostic-probe state machines.
- `client/ui`: terminal state, printable text and bounded canvas rendering.
- `client/app`: input, freeze, capture and replay coordination.
- `tests`: core invariants, socket/lifecycle integration and fuzz entry points.
- `tools/scheduler`: optional CO-RE profiler with an independent privilege boundary.

Keep changes scoped to a tested behavior. Architecture changes should update the
corresponding decision and explain their effect on resource ownership or protocol
semantics. Performance changes should include a reproducer and before/after data.
