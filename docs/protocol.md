# Streaming protocol v2

A frame has a 4-byte unsigned big-endian payload length followed by that many JSON
bytes. Length must be in `1..8,388,608`. There is no native struct layout, padding,
endianness assumption or checksum field on the wire. TCP supplies ordered bytes;
the framing layer recovers message boundaries.

The payload is the same top-level snapshot served by `/metrics`, with:

```json
{
  "protocol_version": 2,
  "type": "snapshot",
  "timestamp": 0,
  "telemetry": {
    "sequence": 1,
    "instance_id": "per-server-run-id",
    "interval_ms": 1000
  }
}
```

The example shows only envelope fields; actual messages also require CPU, memory,
network, disk, process and load sections. `sequence` increases within one server
instance. A changed instance ID resets comparison. Gaps indicate skipped published
generations; they do not necessarily identify where coalescing occurred.

Every frame is a full snapshot. Reconnect requires no delta recovery or historical
replay from the server. The client rejects unsupported versions and invalid required
fields. Optional telemetry fields can be added without changing v2. Removing fields,
changing units or changing the meaning of a field requires a protocol decision.
The old v1 client is not a supported peer for this revision.

The parser limits input bytes, recursion depth, nodes and object keys. It rejects
invalid UTF-8, duplicate object keys, invalid escapes, non-finite numbers and floating-point exponents outside the representable range. JSON
values own all strings. For outbound Linux byte strings, invalid UTF-8 bytes are
escaped as U+00xx to keep the document valid; this is not a lossless byte-identity
encoding for names. Names are presentation fields, not process identifiers.

A frame read deadline begins with its first byte; a peer cannot keep a partial
frame alive by sending occasional bytes forever. Idle timeout is 180 seconds,
covering the maximum configured 60-second sampling interval. The client performs
bounded read work per poll turn so terminal input still runs during a fast stream.
