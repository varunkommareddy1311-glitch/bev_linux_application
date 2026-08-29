// Test 1 & 2 from README testing matrix: calibration loading, invalid
// calibration handling. Deterministic, no live camera/hardware required.
#include <cassert>
#include <cstdio>
#include <filesystem>

#include "bev/Calibration.hpp"

namespace fs = std::filesystem;

int main() {
    // --- valid round-trip ---
    bev::Calibration calib;
    std::array<cv::Point2f, 4> src{
        cv::Point2f(0, 0), cv::Point2f(100, 0), cv::Point2f(100, 100), cv::Point2f(0, 100)};
    std::array<cv::Point2f, 4> dst{
        cv::Point2f(0, 0), cv::Point2f(200, 0), cv::Point2f(200, 200), cv::Point2f(0, 200)};
    calib.setPoints(src, dst, cv::Size(200, 200));
    assert(calib.isValid());
    assert(!calib.homography().empty());

    const fs::path tmpFile = fs::temp_directory_path() / "bev_test_calibration.yaml";
    assert(calib.saveToFile(tmpFile));

    bev::Calibration reloaded;
    assert(reloaded.loadFromFile(tmpFile));
    assert(reloaded.isValid());
    assert(reloaded.outputSize() == cv::Size(200, 200));

    fs::remove(tmpFile);

    // --- invalid: nonexistent file ---
    bev::Calibration missing;
    assert(!missing.loadFromFile("/nonexistent/path/does_not_exist.yaml"));
    assert(!missing.isValid());

    std::printf("test_calibration: OK\n");
    return 0;
}
