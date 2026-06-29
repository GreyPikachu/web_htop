# HTTP API

Numeric IPv4 or IPv6 bind addresses are accepted. Defaults: `127.0.0.1:8080`.
The server accepts GET requests using HTTP/1.0 or HTTP/1.1. HTTP/1.1 requires Host.
Headers are capped at 8 KiB, duplicates are rejected, and request bodies, transfer
encoding, pipelining and keep-alive are not supported. Responses always close the
connection and include Content-Length. There is no CORS browser frontend.

| Route | Success | Failure |
| --- | --- | --- |
| `/health` | 200, reactor alive | No response if the process/I/O loop is unavailable |
| `/ready` | 200 when required collectors are ok and age is below three sampling intervals | 503 while warming, degraded or stale |
| `/metrics` | 200, current full version-2 snapshot | 503 before first publication |
| `/processes` | 200, process section plus sequence, instance_id and truncated | 503 before first publication |
| `/diagnostics` | 200, current reactor counters | — |
| `/exporter` | 200, Prometheus text format | — |

Unknown routes return 404; unsupported methods 405 with Allow: GET. Invalid requests
return 400, oversized headers 431, Expect 417, unsupported HTTP versions 505.
A request that does not complete before its deadline is closed without a response.

`/health` and `/ready` intentionally answer different questions. `/metrics` can
still return the latest known generation while `/ready` is degraded. Check the
published collection timestamps and `/diagnostics` snapshot age.

Diagnostics are monotonic process-lifetime counters plus gauges. The exporter does
not label metrics by PID, session or interface. It avoids creating a new time series
for every transient process/connection. `snapshot_age_ms=-1` means no publication yet.

Authentication and encryption are not implemented in either transport. Loopback is
the default; SSH tunnels protect both ports for remote use. Binding to a public
interface does not add authentication. The systemd unit runs unprivileged.
