#pragma once

#include <filesystem>
#include <string>

#include <opencv2/core.hpp>

#include "bev/Calibration.hpp"
#include "bev/Frame.hpp"
#include "bev/HealthMonitor.hpp"
#include "bev/PerspectiveTransformer.hpp"

namespace bev {

/// Result of processing a single frame. Kept as a small value type (no
/// heap allocation) so it can be returned by value cheaply.
struct ProcessResult {
    bool ok{false};
    std::string errorMessage; // empty when ok == true
};

/// BEVProcessor is the single-camera BEV pipeline entry point and the main
/// public API of bev-core: applications (bev-app) talk to this class and
/// never touch OpenCV calibration/warp calls directly.
///
/// Lifecycle:
///   1. init(calibrationPath)   - load calibration once, precompute homography
///   2. process(input, output) - called once per frame, reuses buffers
///
/// Thread-safety: a single BEVProcessor instance is intended to be used by
/// one producer thread (one camera). Running 4 cameras uses 4 independent
/// BEVProcessor instances (see app/main.cpp / docs/architecture.md).
class BEVProcessor {
public:
    explicit BEVProcessor(Camera camera);

    /// Load calibration from `calibrationPath` (e.g. config/front.yaml) and
    /// prepare the transformer. Must be called exactly once before process().
    bool init(const std::filesystem::path& calibrationPath);

    /// Validate + transform a single input frame into `output`.
    /// - Rejects empty/zero-size input (Layer 1 crash-recovery check).
    /// - Never throws: OpenCV exceptions are caught internally and reported
    ///   via ProcessResult so a single bad frame cannot take down the app.
    /// - `output` is reused across calls; no per-frame allocation once the
    ///   output size stabilizes.
    ProcessResult process(const cv::Mat& input, cv::Mat& output);

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] Camera camera() const noexcept { return camera_; }
    [[nodiscard]] const Calibration& calibration() const noexcept { return calibration_; }
    [[nodiscard]] HealthMonitor& health() noexcept { return health_; }

private:
    Camera camera_;
    Calibration calibration_;
    PerspectiveTransformer transformer_;
    HealthMonitor health_;
    bool initialized_{false};
};

} // namespace bev
