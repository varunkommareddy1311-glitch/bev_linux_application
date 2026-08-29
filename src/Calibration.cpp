#include "bev/Calibration.hpp"

#include <opencv2/core/persistence.hpp>
#include <opencv2/imgproc.hpp>

namespace bev {

bool Calibration::loadFromFile(const std::filesystem::path& yamlPath) {
    cv::FileStorage fs(yamlPath.string(), cv::FileStorage::READ);
    if (!fs.isOpened()) {
        return false;
    }

    std::string cameraName;
    fs["camera"] >> cameraName;

    std::vector<cv::Point2f> src, dst;
    fs["source_points"] >> src;
    fs["dest_points"] >> dst;

    int width = 0, height = 0;
    fs["output_width"] >> width;
    fs["output_height"] >> height;

    fs.release();

    if (src.size() != 4 || dst.size() != 4 || width <= 0 || height <= 0) {
        return false; // malformed calibration file - Layer 1 validation
    }

    std::array<cv::Point2f, 4> srcArr{src[0], src[1], src[2], src[3]};
    std::array<cv::Point2f, 4> dstArr{dst[0], dst[1], dst[2], dst[3]};

    setPoints(srcArr, dstArr, cv::Size(width, height));

    if (!cameraName.empty()) {
        for (Camera cam : kAllCameras) {
            if (toString(cam) == cameraName) {
                camera_ = cam;
                break;
            }
        }
    }

    return valid_;
}

bool Calibration::saveToFile(const std::filesystem::path& yamlPath) const {
    if (!valid_) {
        return false;
    }

    cv::FileStorage fs(yamlPath.string(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        return false;
    }

    fs << "camera" << std::string(toString(camera_));
    fs << "source_points" << std::vector<cv::Point2f>(srcPoints_.begin(), srcPoints_.end());
    fs << "dest_points" << std::vector<cv::Point2f>(dstPoints_.begin(), dstPoints_.end());
    fs << "output_width" << outputSize_.width;
    fs << "output_height" << outputSize_.height;
    fs.release();
    return true;
}

void Calibration::setPoints(const std::array<cv::Point2f, 4>& srcPoints,
                             const std::array<cv::Point2f, 4>& dstPoints,
                             cv::Size outputSize) {
    srcPoints_ = srcPoints;
    dstPoints_ = dstPoints;
    outputSize_ = outputSize;
    recomputeHomography();
}

void Calibration::recomputeHomography() {
    // Computed exactly once per calibration load/update, never per-frame.
    homography_ = cv::getPerspectiveTransform(
        std::vector<cv::Point2f>(srcPoints_.begin(), srcPoints_.end()),
        std::vector<cv::Point2f>(dstPoints_.begin(), dstPoints_.end()));

    valid_ = !homography_.empty() && outputSize_.width > 0 && outputSize_.height > 0;
}

} // namespace bev
