# Calibration

## Workflow

1. Capture (or use) one representative sample image per camera under
   `input/<camera>/`.
2. Run the interactive tool:
   ```
   ./build/select_points input/front/sample.jpg config/front.yaml front 640 640
   ```
3. Click the 4 source points in the image, in a consistent order
   (e.g. top-left, top-right, bottom-right, bottom-left of the region you
   want mapped to the BEV plane). The tool numbers each click 1-4.
4. Press `r` to reset and re-click if you make a mistake.
5. Press `ENTER` once exactly 4 points are selected to save the YAML file.
   Press `ESC` to cancel without saving.
6. Repeat for `rear`, `left`, `right`.

Calibration is a **one-time, offline step**. The runtime application
(`bev-app`) never selects points or recomputes calibration per frame - it
only loads the YAML file once at startup (see `BEVProcessor::init`).

## File format

```yaml
%YAML:1.0
---
camera: "front"
source_points:
   - [ 523., 412. ]
   - [ 1398., 417. ]
   - [ 1690., 1001. ]
   - [ 240., 1005. ]
dest_points:
   - [ 0., 0. ]
   - [ 640., 0. ]
   - [ 640., 640. ]
   - [ 0., 640. ]
output_width: 640
output_height: 640
```

- `source_points`: 4 pixel coordinates in the raw camera image.
- `dest_points`: where those 4 points should map to on the BEV output
  plane (by default, the 4 corners of the output rectangle).
- `output_width` / `output_height`: size of this camera's BEV output.

`bev::Calibration::loadFromFile()` validates that both point arrays have
exactly 4 entries and that the output size is positive before computing
the homography; malformed files are rejected rather than silently
producing a degenerate transform.

## Persisting calibration across restarts

On the target, calibration files are installed to `/etc/bev/*.yaml` by the
`bev-app` Yocto recipe (see `meta-bev/recipes-bev/bev-app/bev-app.bb`).
Because this directory is separate from the application binary and from
`/var/lib/bev` (runtime state), a `systemd` restart or an application
update does not require the operator to recalibrate.

## Known limitations (see also README "Known limitations")

- Four-point homography assumes the mapped region is planar (flat ground).
  It does not model road slope, curbs, or 3D obstacles.
- A generic sample image is **not** sufficient for production automotive
  BEV accuracy. Real calibration requires:
  - the actual camera intrinsics/extrinsics (lens distortion, mounting
    position and orientation relative to the vehicle),
  - calibration targets (e.g. checkerboard patterns) placed at known,
    measured positions relative to the vehicle,
  - per-vehicle recalibration if camera mounting geometry changes.
- This repository's calibration tool selects points on the *raw* image; a
  production pipeline should undistort first (see docs/performance.md,
  "Remap optimization"), then select points on the undistorted image.
