#include "server/WebSocketHandler.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace crsdk_rest {

WebSocketHandler::WebSocketHandler() {
}

WebSocketHandler::~WebSocketHandler() {
    stop();
}

void WebSocketHandler::start(uint16_t port) {
    if (m_running.load()) {
        return;
    }

    m_port = port;
    m_running.store(true);
    std::cout << "[WebSocket] Stub handler started (port " << port << " - not actually listening)\n";
    std::cout << "[WebSocket] Note: Full WebSocket support requires a compatible library\n";
}

void WebSocketHandler::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    std::cout << "[WebSocket] Stub handler stopped\n";
}

void WebSocketHandler::broadcast(const CameraEvent& event) {
    nlohmann::json json;
    json["event"] = event.type;
    json["cameraIndex"] = event.cameraIndex;
    json["data"] = event.data;

    // Format timestamp
    auto time = std::chrono::system_clock::to_time_t(event.timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%FT%TZ");
    json["timestamp"] = ss.str();

    broadcastJson(json.dump());
}

void WebSocketHandler::broadcastJson(const std::string& json) {
    // Log events for debugging (actual WebSocket broadcast disabled)
    std::cout << "[WebSocket Event] " << json << "\n";
}

} // namespace crsdk_rest
