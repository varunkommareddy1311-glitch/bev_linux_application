// Test 10: recovery after a processing error - a single bad/uninitialized
// call must not prevent subsequent good frames from being processed.
#include <cassert>
#include <cstdio>
#include <filesystem>

#include "bev/BEVProcessor.hpp"

namespace fs = std::filesystem;

int main() {
    // Uninitialized processor: process() must fail gracefully, not throw.
    bev::BEVProcessor uninit(bev::Camera::Rear);
    cv::Mat frame(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat output;
    auto result = uninit.process(frame, output);
    assert(!result.ok);
    assert(uninit.health().state() == bev::HealthState::Init);

    // Now initialize properly and confirm normal processing resumes.
    bev::Calibration calib;
    std::array<cv::Point2f, 4> src{
        cv::Point2f(0, 0), cv::Point2f(32, 0), cv::Point2f(32, 32), cv::Point2f(0, 32)};
    std::array<cv::Point2f, 4> dst{
        cv::Point2f(0, 0), cv::Point2f(32, 0), cv::Point2f(32, 32), cv::Point2f(0, 32)};
    calib.setPoints(src, dst, cv::Size(32, 32));
    const fs::path calibFile = fs::temp_directory_path() / "bev_test_recovery_calib.yaml";
    calib.saveToFile(calibFile);

    bev::BEVProcessor recovered(bev::Camera::Rear);
    assert(recovered.init(calibFile));

    // Feed one bad (empty) frame, then a good one - the good frame must
    // still succeed (no cross-frame state corruption from the bad one).
    cv::Mat empty;
    auto badResult = recovered.process(empty, output);
    assert(!badResult.ok);

    auto goodResult = recovered.process(frame, output);
    assert(goodResult.ok);
    assert(recovered.health().state() == bev::HealthState::Running);

    fs::remove(calibFile);
    std::printf("test_recovery: OK\n");
    return 0;
}
