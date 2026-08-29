#pragma once

#include <array>
#include <filesystem>
#include <string>

#include <opencv2/core.hpp>

#include "bev/Frame.hpp"

namespace bev {

/// Holds one camera's calibration: the four source points selected by
/// tools/select_points.cpp, the four destination (BEV-plane) points, and
/// derived data (homography, undistortion maps) needed at runtime.
///
/// Calibration is intentionally decoupled from any single frame: it is
/// loaded once at startup and reused for every frame of that camera.
class Calibration {
public:
    Calibration() = default;

    /// Load calibration for a single camera from a YAML file
    /// (see config/front.yaml for the expected schema).
    /// Returns false (and leaves the object unchanged) on any I/O or
    /// schema error - callers must check this before using the object.
    bool loadFromFile(const std::filesystem::path& yamlPath);

    /// Persist the current calibration back to a YAML file. Used by
    /// tools/select_points.cpp and for round-trip testing.
    bool saveToFile(const std::filesystem::path& yamlPath) const;

    /// Manually set source/destination points (e.g. from the calibration
    /// tool) and recompute the homography.
    void setPoints(const std::array<cv::Point2f, 4>& srcPoints,
                    const std::array<cv::Point2f, 4>& dstPoints,
                    cv::Size outputSize);

    [[nodiscard]] bool isValid() const noexcept { return valid_; }
    [[nodiscard]] Camera camera() const noexcept { return camera_; }
    void setCamera(Camera cam) noexcept { camera_ = cam; }

    [[nodiscard]] const cv::Mat& homography() const noexcept { return homography_; }
    [[nodiscard]] cv::Size outputSize() const noexcept { return outputSize_; }
    [[nodiscard]] const std::array<cv::Point2f, 4>& sourcePoints() const noexcept { return srcPoints_; }
    [[nodiscard]] const std::array<cv::Point2f, 4>& destPoints() const noexcept { return dstPoints_; }

private:
    void recomputeHomography();

    Camera camera_{Camera::Front};
    std::array<cv::Point2f, 4> srcPoints_{};
    std::array<cv::Point2f, 4> dstPoints_{};
    cv::Mat homography_;              // 3x3, CV_64F - computed once, reused every frame
    cv::Size outputSize_{0, 0};       // BEV output plane size for this camera
    bool valid_{false};
};

} // namespace bev
