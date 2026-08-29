#include "bev/BEVProcessor.hpp"

#include <chrono>

#include <opencv2/core.hpp>

namespace bev {

BEVProcessor::BEVProcessor(Camera camera) : camera_(camera) {
    health_.setState(HealthState::Init);
}

bool BEVProcessor::init(const std::filesystem::path& calibrationPath) {
    if (!calibration_.loadFromFile(calibrationPath)) {
        health_.setState(HealthState::Error);
        return false;
    }
    calibration_.setCamera(camera_);

    if (!transformer_.init(calibration_)) {
        health_.setState(HealthState::Error);
        return false;
    }

    initialized_ = true;
    health_.setState(HealthState::Running);
    return true;
}

ProcessResult BEVProcessor::process(const cv::Mat& input, cv::Mat& output) {
    // --- Layer 1: input validation (crash-recovery, docs/recovery.md) ---
    if (!initialized_) {
        health_.onFrameDropped();
        return {false, "BEVProcessor not initialized"};
    }
    if (input.empty() || input.cols <= 0 || input.rows <= 0) {
        health_.onFrameDropped();
        return {false, "empty or invalid input frame"};
    }
    if (!calibration_.isValid() || !transformer_.isInitialized()) {
        health_.onFrameDropped();
        health_.setState(HealthState::Error);
        return {false, "invalid calibration/homography"};
    }

    health_.onFrameArrived();
    const auto start = std::chrono::steady_clock::now();

    // --- Layer 2: recoverable exception boundary ---
    // A single frame's OpenCV exception must not terminate the process or
    // the containing application; it is reported back and the caller
    // decides whether to skip the frame, log, or trigger recovery.
    try {
        if (!transformer_.transform(input, output)) {
            health_.onFrameDropped();
            return {false, "perspective transform failed"};
        }
    } catch (const cv::Exception& e) {
        health_.onFrameDropped();
        health_.setState(HealthState::Recovering);
        return {false, std::string("OpenCV exception: ") + e.what()};
    } catch (const std::exception& e) {
        health_.onFrameDropped();
        health_.setState(HealthState::Recovering);
        return {false, std::string("exception: ") + e.what()};
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    health_.onFrameProcessed(elapsed);
    if (health_.state() != HealthState::Running) {
        health_.setState(HealthState::Running); // recovered
    }

    return {true, {}};
}

} // namespace bev
