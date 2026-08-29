// bev-app: application layer.
//
// This file deliberately contains NO BEV algorithm logic - it only wires
// together bev-core (BEVProcessor), reads/writes files, and handles
// process-level concerns (signals, logging, exit codes). All perspective
// transform / calibration / stitching logic lives in libbev-core.so.
//
// Milestone-1 behavior (see project README, "First executable milestone"):
//   sample image -> calibration -> perspective transform -> BEV output
// implemented here as: process every image in input/<camera>/ and write
// the BEV result to output/<camera>/.

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h> // write(), STDERR_FILENO - POSIX, used only in the async-signal-safe handler

#include <opencv2/imgcodecs.hpp>

#include "bev/BEVProcessor.hpp"
#include "bev/Frame.hpp"

namespace fs = std::filesystem;

namespace {

// --- Layer 3: fatal signal handling -----------------------------------
// Only async-signal-safe operations here: no OpenCV calls, no heap
// allocation, no iostream. We write a fixed message with write(2) and let
// the default handler (or systemd Restart=always) take over afterward.
volatile std::sig_atomic_t g_shutdownRequested = 0;

extern "C" void handleFatalSignal(int signum) {
    const char msg[] = "bev-app: fatal signal received, exiting\n";
    ssize_t written = ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)written; // best-effort; nothing safe to do with this in a signal handler
    std::signal(signum, SIG_DFL);
    std::raise(signum);
}

extern "C" void handleShutdownSignal(int /*signum*/) {
    g_shutdownRequested = 1; // checked in the main loop; safe (sig_atomic_t)
}

void installSignalHandlers() {
    std::signal(SIGSEGV, handleFatalSignal);
    std::signal(SIGABRT, handleFatalSignal);
    std::signal(SIGBUS,  handleFatalSignal);
    std::signal(SIGINT,  handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);
}

bool isImageFile(const fs::path& p) {
    static const std::vector<std::string> exts{".jpg", ".jpeg", ".png", ".bmp"};
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    for (const auto& e : exts) {
        if (ext == e) return true;
    }
    return false;
}

int processCamera(bev::Camera camera, const fs::path& configDir,
                   const fs::path& inputRoot, const fs::path& outputRoot) {
    const fs::path calibPath = configDir / (std::string(bev::toString(camera)) + ".yaml");
    const fs::path inputDir = inputRoot / std::string(bev::toString(camera));
    const fs::path outputDir = outputRoot / std::string(bev::toString(camera));

    bev::BEVProcessor processor(camera);
    if (!processor.init(calibPath)) {
        std::cerr << "[bev-app] ERROR: failed to load calibration for "
                  << bev::toString(camera) << " from " << calibPath << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "[bev-app] " << bev::toString(camera)
              << ": calibration loaded from " << calibPath << "\n";

    if (!fs::exists(inputDir)) {
        std::cerr << "[bev-app] WARNING: input directory does not exist: "
                  << inputDir << " (skipping " << bev::toString(camera) << ")\n";
        return EXIT_SUCCESS;
    }
    std::error_code ec;
    fs::create_directories(outputDir, ec);

    // Preallocated, reused across every frame in this camera's batch -
    // avoids per-frame heap allocation (see docs/performance.md).
    cv::Mat bevOutput;

    std::size_t processedCount = 0, failedCount = 0;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (g_shutdownRequested) break;
        if (!entry.is_regular_file() || !isImageFile(entry.path())) continue;

        cv::Mat input = cv::imread(entry.path().string(), cv::IMREAD_COLOR);
        if (input.empty()) {
            std::cerr << "[bev-app] WARNING: failed to read " << entry.path() << "\n";
            ++failedCount;
            continue;
        }

        const bev::ProcessResult result = processor.process(input, bevOutput);
        if (!result.ok) {
            std::cerr << "[bev-app] WARNING: " << entry.path() << ": "
                      << result.errorMessage << "\n";
            ++failedCount;
            continue;
        }

        fs::path outPath = outputDir / (entry.path().stem().string() + "_bev" +
                                         entry.path().extension().string());
        if (!cv::imwrite(outPath.string(), bevOutput)) {
            std::cerr << "[bev-app] WARNING: failed to write " << outPath << "\n";
            ++failedCount;
            continue;
        }
        ++processedCount;
    }

    std::cout << "[bev-app] " << bev::toString(camera) << ": processed "
              << processedCount << " frame(s), " << failedCount << " failure(s)\n";
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv) {
    installSignalHandlers();

    fs::path configDir = "config";
    fs::path inputRoot = "input";
    fs::path outputRoot = "output";
    if (argc > 1) configDir = argv[1];
    if (argc > 2) inputRoot = argv[2];
    if (argc > 3) outputRoot = argv[3];

    std::cout << "[bev-app] starting: config=" << configDir
              << " input=" << inputRoot << " output=" << outputRoot << "\n";
    std::cout << "[bev-app] OpenCV version: " << CV_VERSION << "\n";

    int rc = EXIT_SUCCESS;
    for (bev::Camera camera : bev::kAllCameras) {
        if (g_shutdownRequested) break;
        // A failure on one camera must not prevent the others from running
        // (see docs/recovery.md, "Do not let an error in one frame
        // automatically terminate the entire application").
        const int camRc = processCamera(camera, configDir, inputRoot, outputRoot);
        if (camRc != EXIT_SUCCESS) rc = camRc;
    }

    std::cout << "[bev-app] shutdown complete\n";
    return rc;
}
