// Test 5, 6, 7: empty image handling, invalid image handling, multiple
// input frames using the same (single-load) calibration.
#include <cassert>
#include <cstdio>
#include <filesystem>

#include "bev/BEVProcessor.hpp"

namespace fs = std::filesystem;

int main() {
    bev::Calibration calib;
    std::array<cv::Point2f, 4> src{
        cv::Point2f(0, 0), cv::Point2f(64, 0), cv::Point2f(64, 64), cv::Point2f(0, 64)};
    std::array<cv::Point2f, 4> dst{
        cv::Point2f(0, 0), cv::Point2f(64, 0), cv::Point2f(64, 64), cv::Point2f(0, 64)};
    calib.setPoints(src, dst, cv::Size(64, 64));
    const fs::path calibFile = fs::temp_directory_path() / "bev_test_processor_calib.yaml";
    calib.saveToFile(calibFile);

    bev::BEVProcessor processor(bev::Camera::Front);
    assert(processor.init(calibFile));
    assert(processor.isInitialized());

    cv::Mat frame(64, 64, CV_8UC3, cv::Scalar(10, 20, 30));
    cv::Mat output;

    // Same calibration reused across many frames - no reload, no reselect.
    for (int i = 0; i < 5; ++i) {
        auto result = processor.process(frame, output);
        assert(result.ok);
    }
    assert(processor.health().framesProcessed() == 5);

    // Empty frame -> rejected, not crashed, counted as dropped.
    cv::Mat emptyFrame;
    auto badResult = processor.process(emptyFrame, output);
    assert(!badResult.ok);
    assert(processor.health().framesDropped() == 1);

    fs::remove(calibFile);
    std::printf("test_bev_processor: OK\n");
    return 0;
}
