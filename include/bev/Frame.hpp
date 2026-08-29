#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace bev {

/// Fixed camera positions for the 4-camera surround-view system.
/// Using enum class + std::array instead of std::map: the camera set is
/// fixed and known at compile time, so we avoid hashing/heap overhead.
enum class Camera : std::uint8_t {
    Front = 0,
    Rear  = 1,
    Left  = 2,
    Right = 3
};

inline constexpr std::array<Camera, 4> kAllCameras{
    Camera::Front, Camera::Rear, Camera::Left, Camera::Right
};

/// Human-readable / config-file name for a camera. Used to resolve
/// config/<name>.yaml and input/<name>/ paths.
constexpr std::string_view toString(Camera cam) noexcept {
    switch (cam) {
        case Camera::Front: return "front";
        case Camera::Rear:  return "rear";
        case Camera::Left:  return "left";
        case Camera::Right: return "right";
    }
    return "unknown";
}

/// Simple frame metadata carried alongside a cv::Mat through the pipeline.
/// Deliberately does not own image data - the cv::Mat is passed alongside
/// this struct (or wrapped by the caller) to keep buffer ownership explicit.
struct FrameMeta {
    Camera camera{Camera::Front};
    std::uint64_t sequenceId{0};
    std::int64_t captureTimestampUs{0}; // monotonic clock, microseconds
};

} // namespace bev
