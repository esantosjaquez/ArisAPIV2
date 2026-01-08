#pragma once

#include <string>
#include <atomic>

// Ensure SSL support is disabled in httplib
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

namespace crsdk_rest {

class CameraManager;

class MjpegStreamer {
public:
    MjpegStreamer();
    ~MjpegStreamer() = default;

    void handleStream(const httplib::Request& req, httplib::Response& res);
    void setTargetFps(int fps) { m_targetFps = fps; }
    int getTargetFps() const { return m_targetFps; }

private:
    std::string generateBoundary();

    std::atomic<int> m_targetFps{30};
};

} // namespace crsdk_rest
