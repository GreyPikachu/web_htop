# Validation record

The implementation was exercised in the authoring Linux environment with GCC 13.3.
This record distinguishes executed checks from optional configurations.

- Debug build with the retained GoogleTest suites: all 21 CTest entries passed.
  The core entry includes 11 groups of invariants; the integration entry exercises
  real loopback sockets, slow clients, capture/replay, FD reclamation and lifecycle.
- AddressSanitizer + UndefinedBehaviorSanitizer: core and integration passed with
  `ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1`.
- LeakSanitizer could not inspect this environment's PID/procfs layout and failed
  before producing a usable leak report. Leak detection was disabled only for the
  local ASan/UBSan execution; the delivered build/CI does not disable it.
- ThreadSanitizer: the concurrent-publication/core test passed. This is not a claim
  that every possible runtime interleaving has been explored.
- The terminal client was driven through a PTY: all six workspaces, filter, freeze
  and clean exit were exercised. Overview/CPU Matrix output was visually inspected.
- The CO-RE profiler requires target-kernel build, verifier and attachment checks.
  Those privileged/kernel-specific checks were not performed here.
- The GCC/Clang CI matrix and libFuzzer targets are supplied as configurations;
  no successful remote CI run or fuzz campaign is claimed by this record.

The standalone script additionally runs a Release build and core/integration tests
on the user's machine by default. It retains the log and attempts source rollback
if that gate fails. Performance numbers should come from scripts/benchmark.py on
the intended deployment hardware, with the report's limitations preserved.
