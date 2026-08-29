// tools/select_points.cpp
//
// Interactive calibration tool. Loads a sample image for one camera,
// lets the operator click 4 source points (numbered 1-4), and saves them
// to a YAML calibration file consumed by bev::Calibration at runtime.
//
// Usage:
//   select_points <image_path> <output_yaml> <camera_name> \
//                 <dst_width> <dst_height>
//
// Controls:
//   left click - add a point (up to 4)
//   r          - reset all selected points
//   ENTER      - finish and save (requires exactly 4 points)
//   ESC        - quit without saving
//
// Destination points are fixed to the four corners of the requested
// output size (a standard "rectify to full frame" BEV mapping); operators
// who need a different destination layout can edit the saved YAML by hand.

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "bev/Calibration.hpp"
#include "bev/Frame.hpp"

namespace {

struct ClickState {
    std::vector<cv::Point2f> points;
    cv::Mat baseImage;
    std::string windowName;
};

void redraw(ClickState& state) {
    cv::Mat display = state.baseImage.clone();
    for (std::size_t i = 0; i < state.points.size(); ++i) {
        cv::circle(display, state.points[i], 6, cv::Scalar(0, 255, 0), -1);
        cv::putText(display, std::to_string(i + 1), state.points[i] + cv::Point2f(8, -8),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
    }
    cv::putText(display, "Click 4 points | r=reset | ENTER=save | ESC=quit",
                cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
    cv::imshow(state.windowName, display);
}

void onMouse(int event, int x, int y, int /*flags*/, void* userdata) {
    auto* state = static_cast<ClickState*>(userdata);
    if (event == cv::EVENT_LBUTTONDOWN && state->points.size() < 4) {
        state->points.emplace_back(static_cast<float>(x), static_cast<float>(y));
        std::cout << "Point " << state->points.size() << ": (" << x << ", " << y << ")\n";
        redraw(*state);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <image_path> <output_yaml> <camera_name> <dst_width> <dst_height>\n";
        return EXIT_FAILURE;
    }

    const std::string imagePath = argv[1];
    const std::string outputYaml = argv[2];
    const std::string cameraName = argv[3];
    const int dstWidth = std::atoi(argv[4]);
    const int dstHeight = std::atoi(argv[5]);

    ClickState state;
    state.windowName = "bev select_points";
    state.baseImage = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (state.baseImage.empty()) {
        std::cerr << "ERROR: failed to load image: " << imagePath << "\n";
        return EXIT_FAILURE;
    }

    cv::namedWindow(state.windowName, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(state.windowName, onMouse, &state);
    redraw(state);

    bool finished = false;
    while (!finished) {
        const int key = cv::waitKey(20);
        switch (key) {
            case 'r':
            case 'R':
                state.points.clear();
                redraw(state);
                break;
            case 13: // ENTER
                if (state.points.size() == 4) {
                    finished = true;
                } else {
                    std::cout << "Need exactly 4 points (have " << state.points.size() << ")\n";
                }
                break;
            case 27: // ESC
                std::cout << "Cancelled - no file written.\n";
                return EXIT_SUCCESS;
            default:
                break;
        }
    }

    std::array<cv::Point2f, 4> srcPts{
        state.points[0], state.points[1], state.points[2], state.points[3]};
    // Destination: full output rectangle, corners in the same
    // top-left/top-right/bottom-right/bottom-left order the operator is
    // expected to click the source points in.
    std::array<cv::Point2f, 4> dstPts{
        cv::Point2f(0.f, 0.f),
        cv::Point2f(static_cast<float>(dstWidth), 0.f),
        cv::Point2f(static_cast<float>(dstWidth), static_cast<float>(dstHeight)),
        cv::Point2f(0.f, static_cast<float>(dstHeight))};

    bev::Calibration calibration;
    calibration.setPoints(srcPts, dstPts, cv::Size(dstWidth, dstHeight));
    for (bev::Camera cam : bev::kAllCameras) {
        if (bev::toString(cam) == cameraName) {
            calibration.setCamera(cam);
            break;
        }
    }

    if (!calibration.saveToFile(outputYaml)) {
        std::cerr << "ERROR: failed to save calibration to " << outputYaml << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Saved calibration to " << outputYaml << "\n";
    return EXIT_SUCCESS;
}
