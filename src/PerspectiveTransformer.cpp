#include "bev/PerspectiveTransformer.hpp"

#include <opencv2/imgproc.hpp>

namespace bev {

bool PerspectiveTransformer::init(const Calibration& calibration) {
    if (!calibration.isValid()) {
        initialized_ = false;
        return false;
    }
    homography_ = calibration.homography().clone(); // one-time copy at init, not per-frame
    outputSize_ = calibration.outputSize();
    initialized_ = true;
    return true;
}

bool PerspectiveTransformer::useRemapPath(const cv::Mat& map1, const cv::Mat& map2) {
    if (map1.empty() || map2.empty()) {
        return false;
    }
    remapX_ = map1;
    remapY_ = map2;
    remapReady_ = true;
    return true;
}

bool PerspectiveTransformer::transform(const cv::Mat& input, cv::Mat& output) const {
    if (!initialized_ || input.empty()) {
        return false;
    }

    if (remapReady_) {
        // Fast path: precomputed remap tables (undistort + perspective
        // folded together). output.create() is a no-op once sizes match.
        output.create(outputSize_, input.type());
        cv::remap(input, output, remapX_, remapY_, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        return true;
    }

    // Default path: per-frame warpPerspective() using the precomputed
    // homography. OpenCV internally reuses `output`'s buffer if it is
    // already the correct size/type.
    cv::warpPerspective(input, output, homography_, outputSize_,
                         cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    return true;
}

} // namespace bev
