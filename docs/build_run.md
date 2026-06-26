# Build and operations

## Requirements

Linux, CMake 3.20 or newer, a C++20 compiler/standard library, Python 3 for tests.
On Ubuntu 24.04 the normal build needs `build-essential cmake python3`.
Tests use loopback sockets and temporary directories. No third-party download is
performed by the default CMake configuration.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWEB_HTOP_BUILD_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure --no-tests=error
./build/server/web_htop_server
```

In another terminal:

```bash
./build/client/web_htop_client localhost 9999 8080
```

Helper scripts use `build` by default; override with `WEB_HTOP_BUILD_DIR` if needed.
CMake presets provide debug, release, ASan and TSan configurations (CMake 3.21+).
GoogleTest codec suites require an installed GTest package and the explicit
`WEB_HTOP_BUILD_LEGACY_TESTS=ON` option. The core/integration suite needs no GTest.

## Server configuration

| Option | Default | Accepted values |
| --- | --- | --- |
| `--bind` | `127.0.0.1` | Numeric IPv4 or IPv6 address |
| `--http-port` | 8080 | 1..65535 |
| `--stream-port` | 9999 | 1..65535, different from HTTP |
| `--interval-ms` | 1000 | 100..60000 |
| `--max-clients` | 256 | 1..4096 |
| `--max-processes` | 1024 | 1..10000 transmitted rows |
| `--request-timeout-ms` | 3000 | 100..60000 |
| `--write-timeout-ms` | 5000 | 100..60000 without write progress |
| `--proc-root` | `/proc` | procfs mount or fixture directory |
| `--sys-root` | `/sys` | sysfs mount or fixture directory |
| `--mount` | `/` | Filesystem capacity probe path |
| `--cgroup` | disabled | Explicit cgroup v2 directory |
| `--include-loopback` | off | Include lo in network metrics |

`WEB_HTOP_HTTP_PORT` and `WEB_HTOP_STREAMING_PORT` remain supported. CLI values take
precedence. Unknown options and malformed/out-of-range numbers fail before startup.

## Service

`cmake --install build` installs binaries under the selected CMake prefix. The
example `packaging/web_htop.service` expects `/usr/local/bin/web_htop_server` and
runs with a dynamic unprivileged user. Review visibility under procfs hidepid and
cgroup permissions; unavailable process data is reported rather than bypassed.
Copy/enable the unit explicitly when deployment is intended. The refactor script
does not install a service, change firewall rules or start a background daemon.

## Troubleshooting

- Bind failure: confirm both ports are free. Startup rolls back the first listener
  if the second cannot bind.
- Ready remains degraded: inspect `/metrics` collector statuses and `/diagnostics`.
- No rates initially: two valid observations are required.
- Old data in the UI: the stream may be idle or reconnecting. The age is independent
  of drawing. Freeze holds an older generation by design.
- Missing cgroup metrics: confirm v2, controller availability and path permissions.
- Invalid hostname: resolution occurs before entering terminal mode; libc controls
  DNS timeout. A numeric address avoids resolver delays.
- A filesystem can stall the collector in a kernel syscall. Network I/O stays
  responsive, but cooperative cancellation cannot interrupt every kernel operation.
