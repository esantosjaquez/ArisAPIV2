#include "server/MjpegStreamer.h"
#include "camera/CameraManager.h"
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <iostream>

namespace crsdk_rest {

MjpegStreamer::MjpegStreamer() {
}

std::string MjpegStreamer::generateBoundary() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    const char* hex = "0123456789abcdef";
    std::string boundary = "----MJPEGBoundary";
    for (int i = 0; i < 16; i++) {
        boundary += hex[dis(gen)];
    }
    return boundary;
}

void MjpegStreamer::handleStream(const httplib::Request& req, httplib::Response& res) {
    // Extract camera index from path
    std::string path = req.path;
    int cameraIndex = -1;

    // Parse /api/v1/cameras/{index}/liveview/stream
    size_t pos = path.find("/cameras/");
    if (pos != std::string::npos) {
        pos += 9;  // length of "/cameras/"
        size_t endPos = path.find("/", pos);
        if (endPos != std::string::npos) {
            try {
                cameraIndex = std::stoi(path.substr(pos, endPos - pos));
            } catch (...) {}
        }
    }

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera || !camera->isConnected()) {
        res.status = 404;
        res.set_content("{\"error\": \"Camera not found or not connected\"}", "application/json");
        return;
    }

    std::string boundary = generateBoundary();
    int targetIntervalMs = 1000 / m_targetFps.load();

    res.set_content_provider(
        "multipart/x-mixed-replace; boundary=" + boundary,
        [this, camera, boundary, targetIntervalMs](size_t /*offset*/, httplib::DataSink& sink) {
            auto frameStart = std::chrono::steady_clock::now();

            auto jpegData = camera->getLiveViewImage();

            if (!jpegData.empty()) {
                // Build frame header
                std::ostringstream header;
                header << "--" << boundary << "\r\n";
                header << "Content-Type: image/jpeg\r\n";
                header << "Content-Length: " << jpegData.size() << "\r\n\r\n";

                std::string headerStr = header.str();

                // Write header
                if (!sink.write(headerStr.data(), headerStr.size())) {
                    return false;  // Client disconnected
                }

                // Write image data
                if (!sink.write(reinterpret_cast<const char*>(jpegData.data()), jpegData.size())) {
                    return false;
                }

                // Write frame terminator
                if (!sink.write("\r\n", 2)) {
                    return false;
                }
            }

            // Frame rate control
            auto frameEnd = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();

            if (elapsed < targetIntervalMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(targetIntervalMs - elapsed));
            }

            return true;  // Continue streaming
        }
    );
}

} // namespace crsdk_rest
