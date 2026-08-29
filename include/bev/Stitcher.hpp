#pragma once

#include <array>

#include <opencv2/core.hpp>

#include "bev/Frame.hpp"

namespace bev {

/// Where each camera's BEV image is placed on the shared 360-degree canvas,
/// and the overlap region (if any) shared with a neighboring camera.
/// Loaded from config alongside each camera's Calibration - kept as a
/// separate struct because placement is a stitching concern, not a
/// per-camera perspective-transform concern.
struct StitchRegion {
    cv::Rect canvasRoi;   // where this camera's BEV image lands on the canvas
    cv::Mat blendMask;    // optional: CV_8UC1 alpha mask for overlap blending
};

/// Combines the four per-camera BEV images into a single 360-degree
/// surround view. Deliberately kept separate from PerspectiveTransformer:
/// perspective correction is a per-camera concern, stitching is a
/// multi-camera composition concern.
///
/// Prototype behavior: calibrated placement of each camera's BEV image into
/// its canvas ROI, with simple linear alpha blending in overlap regions
/// (NOT naive quadrant resize-and-place, per architecture requirements).
///
/// Future extension points (see docs/architecture.md, section 10):
///   - seam-finding (e.g. graph-cut) in overlap regions
///   - exposure/color correction across cameras
///   - geometric alignment refinement using detected overlap features
class Stitcher {
public:
    Stitcher() = default;

    /// Configure canvas size and per-camera placement regions. Must be
    /// called once at startup; regions are reused for every frame set.
    bool init(cv::Size canvasSize,
              const std::array<StitchRegion, kAllCameras.size()>& regions);

    /// Compose the four camera BEV images (indexed by Camera) into
    /// `output`. Any camera image may be empty (camera unavailable) - the
    /// corresponding canvas region is left as-is (typically black or the
    /// last good frame, depending on caller policy).
    bool stitch(const std::array<cv::Mat, kAllCameras.size()>& cameraBevImages,
                cv::Mat& output);

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
    cv::Size canvasSize_{0, 0};
    std::array<StitchRegion, kAllCameras.size()> regions_{};
    bool initialized_{false};
};

} // namespace bev
