#pragma once

#include <string>
#include <mutex>
#include <atomic>
#include <queue>
#include "camera/CameraDeviceWrapper.h"

namespace crsdk_rest {

// Simplified WebSocket handler - logs events for now
// Full WebSocket implementation can be added later with a more modern library
class WebSocketHandler {
public:
    WebSocketHandler();
    ~WebSocketHandler();

    void start(uint16_t port);
    void stop();
    bool isRunning() const { return m_running.load(); }

    void broadcast(const CameraEvent& event);
    void broadcastJson(const std::string& json);
    size_t getConnectionCount() const { return 0; }

private:
    std::atomic<bool> m_running{false};
    uint16_t m_port{8081};
};

} // namespace crsdk_rest
