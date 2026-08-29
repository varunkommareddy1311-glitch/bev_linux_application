# Performance: Memory and Latency

This document states the design intent and measurable targets. It does
**not** claim numbers that haven't been measured on target hardware -
"zero latency" and unverified GPU speedups are explicitly out of scope
(see README "Known limitations").

## Memory model

Per-frame raw buffer sizes at common resolutions (BGR, 3 bytes/pixel):

| Resolution   | Bytes/frame   | Approx. |
|--------------|---------------|---------|
| 1920x1080    | 1920*1080*3   | ~6.2 MB |
| 1280x720     | 1280*720*3    | ~2.6 MB |
| 640x480      | 640*480*3     | ~0.9 MB |

For a 4-camera system at 1920x1080 capture resolution, **capture buffers
alone** require ~25 MB resident, before accounting for:
- 4x per-camera BEV output buffers (size depends on calibration, typically
  smaller than capture resolution, e.g. 640x640 => ~1.2 MB each => ~5 MB),
- the 4x undistort/remap maps if the remap fast path is enabled (2x
  `CV_32FC1` maps per camera at capture resolution: `1920*1080*4*2` ≈
  16.6 MB per camera, ~66 MB for 4 cameras - remap maps are the largest
  fixed memory cost of the optimized path and must be budgeted for),
- the stitched 360-degree canvas buffer,
- any capture/display double-buffering the platform's camera/display
  stack requires,
- bounded processing queues (see "Queueing policy" below).

### Memory-efficiency rules implemented in this codebase

- `PerspectiveTransformer::transform()` and `Stitcher::stitch()` call
  `cv::Mat::create()` on their output parameter rather than returning a
  newly constructed `cv::Mat`; `create()` is a no-op once the size/type
  match, so no heap allocation occurs after the first frame.
- `BEVProcessor::process(const cv::Mat& input, cv::Mat& output)` takes the
  output by reference for the same reason - see the API note in
  `include/bev/BEVProcessor.hpp`.
- `app/main.cpp` allocates one `cv::Mat bevOutput` per camera batch and
  reuses it across every image in `input/<camera>/`.
- No `cv::Mat::clone()` calls in the hot path; `Stitcher::stitch()` only
  allocates a temporary when a source image needs resizing to fit its
  canvas ROI.
- Calibration and homography are loaded/computed once at `init()`, not
  per frame.
- No disk I/O in the runtime processing path except the batch app's
  explicit `imwrite()` per output frame (this is direct output, not
  incidental I/O); a future live-camera integration should not add
  incidental disk writes to the per-frame path except in an explicit debug
  mode.

## Latency model

Do not assume "zero latency" - it is not a real requirement. Define and
measure each stage separately:

```
capture latency -> BEV processing latency -> stitching latency -> display latency
                    \_______________________ end-to-end latency ________________/
```

Frame period budget:

| Target FPS | Frame period |
|-----------:|-------------:|
| 30 FPS     | ~33.3 ms     |
| 60 FPS     | ~16.7 ms     |

`bev::HealthMonitor::onFrameProcessed()` records the last per-frame
processing latency (`lastLatencyUs()`), giving a cheap, allocation-free
hook for logging/telemetry without adding per-frame overhead of its own.

### Queueing policy

For a real-time visualization system, a bounded, "latest frame wins" queue
policy is preferred over an unbounded FIFO: stale frames are less useful
than the current one, and an unbounded queue is both a latency hazard and
a memory-growth hazard. This repository's batch-processing `bev-app` has
no live queue (it processes files sequentially); a future live-camera
input source should implement this policy at the capture/queue boundary,
not inside `bev-core`.

## Remap optimization path (planned, not yet enabled by default)

`PerspectiveTransformer` supports switching from `cv::warpPerspective()`
(recomputes the full remap on the fly each call, works directly from the
homography) to `cv::remap()` with precomputed `map1`/`map2` tables
(`useRemapPath()`), which is expected to be cheaper for the fixed,
repeated transform used at runtime.

```
Startup:
    Load calibration
    Generate maps (cv::initUndistortRectifyMap() + homography fold-in)
    Allocate buffers

Runtime:
    Capture frame
    remap()
    process BEV
    stitch
    display
```

**Benchmark `warpPerspective()` vs. `remap()` on the actual target SoC**
before relying on this path for a performance claim - the relative cost
depends on CPU architecture, cache behavior, and whether OpenCV was built
with SIMD/NEON optimizations for that target.

## What still needs target-hardware measurement

- warpPerspective vs. remap on the target SoC
- Actual end-to-end latency at 30/60 FPS with real camera capture
- Whether CPU-only OpenCV processing meets the latency budget for 4
  cameras at production resolution, or whether SoC hardware
  warp/scaler blocks, SIMD, or GPU offload are required
