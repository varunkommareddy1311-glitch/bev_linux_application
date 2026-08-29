# BEV Architecture

## 1. Layered software architecture

```
+-----------------------------+
|          bev-app            |
|      Application layer      |
|  (I/O, signals, logging,    |
|   directory batch loop)     |
+--------------+--------------+
               |
               v
+-----------------------------+
|          bev-core           |
|       Reusable library      |
|  (libbev-core.so)           |
+-----------------------------+
               |
      +--------+--------+
      |        |        |
      v        v        v
Calibration  BEV      Stitcher
             Engine
               |
               v
            OpenCV
```

`bev-app` contains **no BEV algorithm logic**. It only:
- reads calibration paths / input-output directories from argv,
- drives `bev::BEVProcessor` per camera,
- handles process-level concerns: signals, exit codes, structured logging.

All perspective-transform, calibration, and stitching logic lives in
`bev-core`, so it can be reused by a future live-camera application, a test
harness, or a different application front-end without modification.

## 2. Vehicle camera layout

```
        FRONT
          |
          |
LEFT --- CAR --- RIGHT
          |
          |
        REAR
```

Each of the four cameras has an independent `Calibration` (own YAML file,
own homography) and its own `BEVProcessor` instance.

## 3. End-to-end target pipeline

```
Camera frames
     |
     v
Camera calibration
     |
     v
Distortion correction        (planned; see docs/performance.md)
     |
     v
Perspective / BEV transformation   (implemented: PerspectiveTransformer)
     |
     v
Individual camera BEV
     |
     v
Multi-camera stitching        (implemented, prototype: Stitcher)
     |
     v
Blending / seam handling      (prototype alpha blend; seam-finding planned)
     |
     v
360-degree BEV
     |
     v
Display / HMI                 (out of scope for this repo)
```

## 4. Per-camera perspective transform pipeline

```
Source camera image
      |
      v
4 source points   (tools/select_points.cpp, operator-selected once)
      |
      v
4 destination points   (fixed output rectangle corners)
      |
      v
cv::getPerspectiveTransform()     <- computed once, at calibration load
      |
      v
Homography matrix H
      |
      v
cv::warpPerspective()             <- applied every frame, reuses H
      |
      v
BEV image
```

The homography is **not** recomputed per frame. `Calibration::loadFromFile()`
computes it once; `PerspectiveTransformer::init()` copies it once;
`PerspectiveTransformer::transform()` only calls `warpPerspective()` per
frame, reusing the same 3x3 matrix and the same output buffer.

## 5. Four-camera + stitching architecture

```
Front -> H_front -> Front BEV  --\
Rear  -> H_rear  -> Rear BEV   ---\
Left  -> H_left  -> Left BEV   ----> Stitcher -> 360-degree BEV
Right -> H_right -> Right BEV  --/
```

Each camera's `BEVProcessor` runs independently (own calibration, own
buffers, own `HealthMonitor`). The `Stitcher` places each camera's BEV
image into a calibrated `StitchRegion` on a shared canvas and blends
overlaps - it does not naively resize four images into quadrants (see
`include/bev/Stitcher.hpp`).

## 6. Crash-recovery layering

```
BEV Application
     |
     v
input validation        (Layer 1: BEVProcessor::process rejects
     |                    empty/invalid frames and invalid calibration)
     v
exception handling       (Layer 2: try/catch around the transform call;
     |                    a single bad frame reports failure, does not throw)
     v
health monitoring        (Layer 6: HealthMonitor heartbeat/state, polled by
     |                    a future watchdog integration)
     v
systemd                  (Layer 5: Restart=always, RestartSec=2)
     |
     v
automatic restart
```

Layer 3 (fatal signal handling for `SIGSEGV`/`SIGABRT`/`SIGBUS`) and Layer 4
(calibration persisted under `/etc/bev`, independent of the running
process) are implemented in `app/main.cpp` and the Yocto `bev-app` recipe
respectively. See `docs/recovery.md` for the full breakdown.

## 7. Build-time module boundaries

```
bev-project/
  include/bev/, src/     -> bev-core (portable, no Yocto/app dependency)
  app/                    -> bev-app (thin application shell)
  tools/                  -> developer tooling, links bev-core
  meta-bev/               -> Yocto packaging only; no C++ logic
```

`bev-core` is buildable and testable on a desktop Linux machine with no
Yocto toolchain, and the same source is cross-compiled unmodified for the
target via the `meta-bev` recipes.
