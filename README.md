# bev-project

Automotive Bird's-Eye-View (BEV) surround-view application in C++17 /
OpenCV, targeting an embedded Linux / Yocto environment.

> **Prototype status:** this repository implements the first executable
> milestone (single camera: sample image -> calibration -> perspective
> transform -> BEV output) plus the full architecture, build system, test
> scaffolding, crash-recovery layers, and Yocto packaging needed to grow
> it into a 4-camera production surround-view system. See "Known
> limitations" below for exactly what is and isn't production-ready.

---

## 1. Project overview

This project processes camera images into a bird's-eye (top-down) view
using OpenCV perspective transformation, with an architecture designed to
scale from single static images to a live 4-camera automotive
surround-view system on an embedded Linux/Yocto target.

## 2. Objectives

- Process sample images from an input directory into BEV output (working
  today).
- Support 4 cameras (front/rear/left/right), each independently
  calibrated.
- Combine the 4 camera BEVs into a 360-degree surround view.
- Run reliably, unattended, on an embedded automotive Linux target, with
  layered crash recovery and low, measured, memory/latency budgets.
- Keep the BEV algorithm core (`bev-core`) reusable independently of both
  the application shell (`bev-app`) and the Yocto packaging (`meta-bev`).

## 3. Architecture diagram

See [`docs/architecture.md`](docs/architecture.md) for the full set of
diagrams (layered architecture, camera layout, end-to-end pipeline,
crash-recovery layering). Summary:

```
bev-app (application layer: I/O, signals, logging)
   |
   v
bev-core (libbev-core.so: Calibration, PerspectiveTransformer,
           BEVProcessor, Stitcher, HealthMonitor)
   |
   v
OpenCV
```

```
Camera frames -> calibration -> distortion correction (planned) ->
perspective/BEV transform -> per-camera BEV -> stitching ->
blending/seams -> 360-degree BEV -> display/HMI (out of scope here)
```

## 4. Repository structure

```
bev-project/
  CMakeLists.txt
  README.md
  LICENSE
  include/bev/          Public headers (BEVProcessor, Calibration, ...)
  src/                   bev-core implementation
  app/main.cpp            bev-app application shell
  tools/select_points.cpp Interactive calibration tool
  tests/                  Unit/integration tests (ctest)
  config/                 Per-camera calibration YAML
  input/<camera>/         Sample input images (front/rear/left/right)
  output/                 Batch-processed BEV output
  systemd/bev.service     systemd unit (Layer 5 crash recovery)
  docs/                   architecture.md, calibration.md,
                          performance.md, recovery.md
  meta-bev/               Yocto layer (conf/, recipes-bev/)
```

## 5. Software requirements

- C++17 compiler (GCC 9+ / Clang 10+)
- CMake >= 3.16
- OpenCV >= 4.x (`core imgproc imgcodecs highgui calib3d` components)
- pthreads (via CMake `Threads`)
- (Yocto path) a Yocto/OpenEmbedded build with `meta-openembedded` for the
  `opencv` recipe

## 6. Desktop build instructions

```bash
mkdir build && cd build
cmake -DBUILD_BEV_APP=ON -DBUILD_BEV_TOOLS=ON -DBUILD_BEV_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure
```

## 7. OpenCV dependency

On Ubuntu/Debian:

```bash
sudo apt-get install libopencv-dev
```

`CMakeLists.txt` uses `find_package(OpenCV REQUIRED COMPONENTS core
imgproc imgcodecs highgui calib3d)` and links only those components into
`bev-core`/`bev-app`.

## 8. CMake build

Top-level options:

| Option            | Default | Purpose                                |
|--------------------|:------:|-----------------------------------------|
| `BUILD_BEV_APP`    | ON     | Build the `bev-app` executable          |
| `BUILD_BEV_TOOLS`  | ON     | Build `select_points`                   |
| `BUILD_BEV_TESTS`  | OFF    | Build and register `ctest` unit tests   |

`bev-core` is always built as `libbev-core.so`. `install()` targets place
the library/headers under the standard `lib`/`include` prefixes and
calibration YAML under `/etc/bev` for packaging.

## 9. Running the calibration tool

```bash
./build/select_points input/front/sample.jpg config/front.yaml front 640 640
```

Loads `sample.jpg`, opens an OpenCV window, and waits for 4 mouse clicks.

## 10. Selecting source points

- **Left click**: add a point (numbered 1-4 as you click).
- **`r`**: reset all selected points and start over.
- **ENTER**: finish and save (requires exactly 4 points selected).
- **ESC**: quit without saving.

Click points in a consistent order (e.g. top-left, top-right,
bottom-right, bottom-left of the ground region you want rectified) - the
tool maps them, in that order, to the 4 corners of the output rectangle.

## 11. Calibration file format

See [`docs/calibration.md`](docs/calibration.md). Summary:

```yaml
camera: "front"
source_points: [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]
dest_points:   [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]
output_width: 640
output_height: 640
```

## 12. Running single-camera BEV

```bash
./build/bev-app config input output
```

Loads `config/front.yaml` (and rear/left/right), processes every image in
`input/front/` (etc.), and writes `output/front/<name>_bev.jpg`.

## 13. Processing an input directory

Every image file (`.jpg`/`.jpeg`/`.png`/`.bmp`) in `input/<camera>/` is
processed using that camera's single, already-loaded calibration - the
homography is **not** recomputed and points are **not** reselected per
image (see `app/main.cpp::processCamera`).

## 14. Four-camera BEV architecture

Each camera (`front`/`rear`/`left`/`right`) gets its own `Calibration`
file and its own `BEVProcessor` instance, run independently so a failure
processing one camera's frame does not stop the others (see
`app/main.cpp::main`, which continues to the next camera even if one
fails). See `docs/architecture.md` section 5 for the diagram.

## 15. Stitching architecture

`bev::Stitcher` places each camera's BEV image into a calibrated
`StitchRegion` on a shared canvas, with optional per-region alpha-blend
masks for overlaps - not naive quadrant resize-and-place. Seam-finding,
exposure correction, and geometric alignment refinement are documented,
not-yet-implemented extension points (`include/bev/Stitcher.hpp`).

## 16. Memory optimization

See [`docs/performance.md`](docs/performance.md) for the full memory
model and byte-level budget table. Key rules already implemented:
output buffers are reused via `cv::Mat::create()` (a no-op once
size/type match), no per-frame `clone()`, calibration/homography computed
once at init, no incidental disk I/O in the runtime processing path.

## 17. Latency optimization

See `docs/performance.md`. 30 FPS => ~33.3 ms/frame budget, 60 FPS =>
~16.7 ms/frame budget. `HealthMonitor` records per-frame processing
latency for measurement. A "latest frame wins", bounded-queue policy is
recommended for any future live-camera input source. **No latency numbers
are claimed until measured on the target SoC.**

## 18. Crash recovery

See [`docs/recovery.md`](docs/recovery.md) for the full 6-layer
breakdown (input validation -> exception handling -> fatal signal
handling -> persisted calibration -> systemd restart -> health
monitoring), each mapped to the exact file/function that implements it.

## 19. Health monitoring

`bev::HealthMonitor` (header-only, atomic-based) tracks `HealthState`
(`Init`/`Running`/`Error`/`Recovering`/`Shutdown`), frame arrival/drop/
processed counters, and last latency/heartbeat timestamps per
`BEVProcessor` (i.e. per camera). See `include/bev/HealthMonitor.hpp`.

## 20. systemd integration

`systemd/bev.service` runs `bev-app` with `Restart=always`,
`RestartSec=2`, a bounded restart-rate limit, and `StateDirectory=bev`
for runtime working data. Installed and enabled by the `bev-app` Yocto
recipe.

## 21. Yocto meta-layer integration

```bash
bitbake-layers create-layer meta-bev   # if starting from scratch
bitbake-layers add-layer meta-bev
bitbake bev-core
bitbake bev-app
```

Add `bev-app` to your image's `IMAGE_INSTALL` to include it (this also
pulls in `bev-core` via `RDEPENDS`). See `meta-bev/recipes-bev/*/*.bb` -
each recipe uses `inherit cmake` and passes the same `BUILD_BEV_*` CMake
options as the desktop build, so the identical source tree builds for
both.

## 22. Testing

```bash
cmake -DBUILD_BEV_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure
```

Current coverage (`tests/`): calibration load/save round-trip and
invalid-file rejection, homography generation + perspective transform +
buffer-reuse, multi-frame processing with a single loaded calibration,
and recovery after a bad frame. All tests use synthetic in-memory images
- **no live camera hardware required**. See `docs/architecture.md` for
which of the 12 items in the original test-strategy list are covered
today vs. planned (four-camera integration and stitching tests are
structured but not yet populated with fixtures).

## 23. Profiling

Not yet wired into the build. Recommended starting points for the
target SoC: `perf record`/`perf report` for CPU hotspots, and OpenCV's
`cv::TickMeter` or the `HealthMonitor` latency hook around
`BEVProcessor::process()` for per-frame timing without external tools.

## 24. Debugging

Build with `-DCMAKE_BUILD_TYPE=Debug` for symbols. `bev-app` logs
startup, OpenCV version, calibration loading, and per-camera
processed/failed frame counts to stdout/stderr - redirect these to a
file or `journalctl` (via systemd) for post-mortem review after a crash
(Layer 3 signal handler also writes a fixed message to stderr before
re-raising).

## 25. Git workflow

Development branch: `feature/bev-opencv`. Suggested commit sequence
(each intended to be independently buildable where practical):

```
bev: add OpenCV project skeleton
bev: add sample image input
bev: add interactive calibration tool
bev: add perspective transformation
bev: add persistent calibration
bev: add batch image processing
bev: add distortion correction
bev: add four camera support
bev: add stitching
bev: add vehicle overlay
bev: add memory optimization
bev: add health monitoring
bev: add crash recovery
bev: add systemd service
bev: add Yocto meta-layer
bev: add BEV tests
```

## 26. Future enhancements

- Live camera input via V4L2/GStreamer, behind the same `BEVProcessor`
  API (no `bev-core` changes required - only a new frame source feeding
  `process()`).
- Lens distortion correction (`cv::initUndistortRectifyMap`) folded into
  the remap fast path (`PerspectiveTransformer::useRemapPath`).
- Seam-finding, exposure correction, and geometric-alignment refinement
  in `Stitcher`.
- Hardware watchdog (`/dev/watchdog`) integration on top of the existing
  `HealthMonitor` hooks.
- Per-camera process isolation (one service per camera) if stronger
  fault isolation than "whole `bev-app` restarts" is required.
- CPU profiling and SIMD/GPU/SoC-hardware-warp evaluation once running on
  target hardware.

## 27. Known limitations

- A four-point homography assumes a **planar** ground surface; it does
  not model road slope, curbs, or 3D obstacles.
- The example `config/*.yaml` files use placeholder points from a generic
  sample image - they are **not** valid for any real vehicle and must be
  replaced via `tools/select_points.cpp` on real, ideally undistorted,
  camera footage.
- Real camera intrinsic/extrinsic calibration (not just 4 clicked points)
  is required for production accuracy; lens distortion must be corrected
  for real cameras.
- Camera synchronization across the 4 cameras and exposure differences
  between them are not handled in this prototype.
- Stitching overlap regions require calibrated overlap geometry per
  vehicle/camera-mounting configuration; `Stitcher`'s blend mask support
  is a prototype hook, not a calibrated production overlap solution.
- Vehicle dimensions and camera mounting geometry directly affect BEV
  quality and are out of scope for this repository's calibration tool.
- Real-time performance (see `docs/performance.md`) has **not** been
  measured on any target SoC; do not treat the frame-period tables as
  achieved performance.
- "Zero latency" is not a realistic requirement anywhere in this design;
  only a measurable, budgeted latency is meaningful.
- CPU-only OpenCV processing (`warpPerspective`) may not be sufficient
  for production 4-camera, high-resolution, real-time BEV; hardware
  acceleration or zero-copy capture/display paths may be required
  depending on the target SoC, and must be evaluated on that SoC.
- This sandbox's OpenCV `imgcodecs`/`highgui` components transitively
  depend on a system `libgdal` build with broken `libpq`/`libmysqlclient`
  dependencies (unrelated to this project), which prevented linking
  `bev-app`/`select_points` in *this specific development sandbox*.
  `libbev-core.so` itself builds and links cleanly, and all `bev-core`
  unit tests pass via `ctest`. This is expected to build cleanly on a
  normal desktop Linux dev machine or the Yocto target's OpenCV recipe.
