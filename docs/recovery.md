# Crash Recovery

Recovery is implemented in layers, from cheapest/most-local to
most-drastic. Each layer is implemented in a specific, identifiable place
in the codebase so it can be tested and reasoned about independently.

## Layer 1 - Input validation

**Where:** `BEVProcessor::process()` (`src/BEVProcessor.cpp`)

Checks before any OpenCV call:
- processor is initialized,
- input frame is non-empty and has positive dimensions,
- calibration and homography are valid.

Invalid input is rejected and reported via `ProcessResult{false, ...}`
plus `HealthMonitor::onFrameDropped()`. It never reaches `warpPerspective()`.

Tested by: `tests/test_bev_processor.cpp` (empty-frame case),
`tests/test_calibration.cpp` (malformed/missing calibration file).

## Layer 2 - Recoverable exceptions

**Where:** `BEVProcessor::process()`, around the call into
`PerspectiveTransformer::transform()`.

`cv::Exception` and `std::exception` are caught at this boundary. A single
frame's exception:
- is reported back to the caller via `ProcessResult`,
- moves `HealthMonitor` state to `Recovering`,
- does **not** propagate up and terminate `bev-app`.

The next successful frame moves the state back to `Running`
(`BEVProcessor::process()`, end of the try block).

Tested by: `tests/test_recovery.cpp` (bad frame followed by a good one).

## Layer 3 - Fatal signal handling

**Where:** `app/main.cpp` (`installSignalHandlers()`).

`SIGSEGV`, `SIGABRT`, `SIGBUS` are handled with a minimal,
async-signal-safe handler: a fixed-size `write(2)` to stderr, then
re-raising the signal with the default disposition so the process
actually terminates (and the OS/systemd sees a real crash, not a hang).

**Deliberately does not** call any OpenCV, `iostream`, or heap-allocating
function from the handler - none of those are async-signal-safe, and
calling them from a signal handler mid-crash can deadlock or corrupt
state further.

`SIGINT`/`SIGTERM` are handled separately (`handleShutdownSignal`) via a
`sig_atomic_t` flag checked in the main processing loop, for a clean,
non-abrupt shutdown path (not a "recovery" case, but avoids conflating
graceful shutdown with a crash).

## Layer 4 - Calibration persisted independently

**Where:** `meta-bev/recipes-bev/bev-app/bev-app.bb` installs
`config/*.yaml` to `/etc/bev/`, separate from the application binary and
from `/var/lib/bev` runtime state.

A process restart (Layer 5) reads calibration from `/etc/bev` again - it
does not need to be recreated or reselected.

## Layer 5 - systemd automatic restart

**Where:** `systemd/bev.service`.

```ini
Restart=always
RestartSec=2
StartLimitIntervalSec=60
StartLimitBurst=10
```

If the process exits (including via Layer 3's fatal-signal path),
systemd restarts it after 2 seconds. `StartLimitBurst`/
`StartLimitIntervalSec` cap the restart rate so a persistently crashing
build doesn't spin the CPU or spam logs indefinitely - once the burst
limit is hit, the unit is left failed for operator/watchdog attention
rather than restart-looping forever.

## Layer 6 - Health / heartbeat monitoring

**Where:** `include/bev/HealthMonitor.hpp`, `src/HealthMonitor.cpp`.

Tracks, per `BEVProcessor` instance (i.e. per camera):
- `HealthState`: `Init -> Running -> Error/Recovering -> Running`,
- frames arrived / dropped / processed (atomic counters),
- last processing latency,
- last heartbeat timestamp, with `isStale(timeoutUs)` for a future
  watchdog to poll.

This is intentionally simple in the initial prototype: no threads, no
external dependencies, and it does not itself decide to restart anything
- it exposes state for whatever *does* make that decision (a future
watchdog, or systemd via a to-be-added `sd_notify` heartbeat integration).
The architecture requirement was explicit that an overly complex watchdog
should not be built in the initial prototype.

## What is intentionally out of scope for the prototype

- A dedicated hardware watchdog integration (`/dev/watchdog`) - `systemd`
  restart plus the `HealthMonitor` hooks are the foundation for this, but
  wiring an actual watchdog device is a target-BSP-specific follow-up.
- Per-camera independent process isolation (currently all 4 cameras run
  in one `bev-app` process; a crash affects all 4 until systemd restarts).
  Splitting into 4 independent processes/services is a reasonable future
  evolution if per-camera fault isolation at the process level is needed.
