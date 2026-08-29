#include "bev/Stitcher.hpp"

#include <opencv2/imgproc.hpp>

namespace bev {

bool Stitcher::init(cv::Size canvasSize,
                     const std::array<StitchRegion, kAllCameras.size()>& regions) {
    if (canvasSize.width <= 0 || canvasSize.height <= 0) {
        return false;
    }
    for (const auto& region : regions) {
        if (region.canvasRoi.width <= 0 || region.canvasRoi.height <= 0) {
            return false;
        }
    }
    canvasSize_ = canvasSize;
    regions_ = regions;
    initialized_ = true;
    return true;
}

bool Stitcher::stitch(const std::array<cv::Mat, kAllCameras.size()>& cameraBevImages,
                       cv::Mat& output) {
    if (!initialized_) {
        return false;
    }

    // output is reused across calls; create() is a no-op once allocated.
    output.create(canvasSize_, CV_8UC3);

    for (std::size_t i = 0; i < kAllCameras.size(); ++i) {
        const cv::Mat& src = cameraBevImages[i];
        if (src.empty()) {
            continue; // camera unavailable this frame - leave canvas region untouched
        }

        const cv::Rect& roi = regions_[i].canvasRoi;
        const cv::Rect clampedRoi = roi & cv::Rect(cv::Point(0, 0), canvasSize_);
        if (clampedRoi.width <= 0 || clampedRoi.height <= 0) {
            continue; // misconfigured region - skip rather than crash
        }

        cv::Mat dstView = output(clampedRoi);
        cv::Mat srcResized;
        const cv::Mat* srcToCopy = &src;
        if (src.size() != clampedRoi.size()) {
            cv::resize(src, srcResized, clampedRoi.size(), 0, 0, cv::INTER_LINEAR);
            srcToCopy = &srcResized;
        }

        const cv::Mat& mask = regions_[i].blendMask;
        if (!mask.empty() && mask.size() == clampedRoi.size()) {
            // Simple linear alpha blend in the overlap region. Replace with
            // seam-finding / exposure correction for production use
            // (docs/architecture.md, section 10).
            srcToCopy->copyTo(dstView, mask);
        } else {
            srcToCopy->copyTo(dstView);
        }
    }

    return true;
}

} // namespace bev
