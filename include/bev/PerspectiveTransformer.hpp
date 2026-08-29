#pragma once

#include <opencv2/core.hpp>

#include "bev/Calibration.hpp"

namespace bev {

/// Applies a precomputed homography to a frame to produce its single-camera
/// BEV image. This class does NOT recompute the homography per frame - it
/// is handed a valid, already-calibrated Calibration object at init time.
///
/// Current implementation: cv::warpPerspective() per frame.
/// Planned optimization path (see docs/performance.md): precompute an
/// undistort+perspective remap table once via cv::initUndistortRectifyMap()
/// combined with the homography, then use cv::remap() at runtime, which is
/// typically cheaper than warpPerspective() for a fixed, repeated transform.
class PerspectiveTransformer {
public:
    PerspectiveTransformer() = default;

    /// Bind to a calibration. Precomputes anything reusable (currently just
    /// validates and caches the homography/output size; remap-table
    /// generation is a documented extension point, see useRemapPath()).
    bool init(const Calibration& calibration);

    /// Enable the remap-based fast path once remap tables are available.
    /// No-op / returns false in this prototype (see docs/performance.md);
    /// kept as an explicit extension point so callers can opt in later
    /// without changing call sites.
    bool useRemapPath(const cv::Mat& map1, const cv::Mat& map2);

    /// Transform `input` into `output`. `output` is reused across calls
    /// (cv::Mat::create() is a no-op if the size/type already match), so
    /// no per-frame heap allocation occurs after the first call.
    /// Returns false if not initialized or if `input` is invalid.
    bool transform(const cv::Mat& input, cv::Mat& output) const;

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] cv::Size outputSize() const noexcept { return outputSize_; }

private:
    cv::Mat homography_;     // 3x3 CV_64F, copied from Calibration at init()
    cv::Size outputSize_{0, 0};

    // Remap fast-path (optional, disabled by default in this prototype).
    cv::Mat remapX_;
    cv::Mat remapY_;
    bool remapReady_{false};

    bool initialized_{false};
};

} // namespace bev
