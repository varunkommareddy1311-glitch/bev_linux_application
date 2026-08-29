// Test 3 & 4: homography generation, perspective transformation, plus
// buffer-reuse check (test 11: same `output` Mat reused across calls
// without reallocation once sizes stabilize).
#include <cassert>
#include <cstdio>

#include "bev/Calibration.hpp"
#include "bev/PerspectiveTransformer.hpp"

int main() {
    bev::Calibration calib;
    std::array<cv::Point2f, 4> src{
        cv::Point2f(10, 10), cv::Point2f(90, 10), cv::Point2f(90, 90), cv::Point2f(10, 90)};
    std::array<cv::Point2f, 4> dst{
        cv::Point2f(0, 0), cv::Point2f(128, 0), cv::Point2f(128, 128), cv::Point2f(0, 128)};
    calib.setPoints(src, dst, cv::Size(128, 128));
    assert(calib.isValid());

    bev::PerspectiveTransformer transformer;
    assert(transformer.init(calib));
    assert(transformer.isInitialized());

    cv::Mat input(100, 100, CV_8UC3, cv::Scalar(50, 60, 70));
    cv::Mat output;
    assert(transformer.transform(input, output));
    assert(output.size() == cv::Size(128, 128));
    assert(output.type() == CV_8UC3);

    // Reuse check: same underlying buffer pointer after a second call with
    // the same-sized input (cv::Mat::create() is a no-op in that case).
    const void* firstDataPtr = output.data;
    cv::Mat input2(100, 100, CV_8UC3, cv::Scalar(1, 2, 3));
    assert(transformer.transform(input2, output));
    assert(output.data == firstDataPtr);

    // Empty input must be rejected, not crash.
    cv::Mat empty;
    assert(!transformer.transform(empty, output));

    std::printf("test_perspective: OK\n");
    return 0;
}
