#include "server/RestServer.h"
#include "server/WebSocketHandler.h"
#include "server/MjpegStreamer.h"
#include "api/ApiRouter.h"
#include <iostream>

namespace crsdk_rest {

RestServer::RestServer(const std::string& host, int port, int wsPort)
    : m_host(host)
    , m_port(port)
    , m_wsPort(wsPort)
    , m_httpServer(std::make_unique<httplib::Server>())
    , m_wsHandler(std::make_unique<WebSocketHandler>())
    , m_mjpegStreamer(std::make_unique<MjpegStreamer>())
{
}

RestServer::~RestServer() {
    stop();
}

bool RestServer::start() {
    if (m_running.load()) {
        return true;
    }

    // Setup routes
    setupRoutes();

    // Start WebSocket server
    m_wsHandler->start(static_cast<uint16_t>(m_wsPort));

    // Start HTTP server in background thread
    m_running.store(true);
    m_httpThread = std::thread(&RestServer::runHttpServer, this);

    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return m_running.load();
}

void RestServer::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    // Stop HTTP server
    if (m_httpServer) {
        m_httpServer->stop();
    }

    // Stop WebSocket server
    if (m_wsHandler) {
        m_wsHandler->stop();
    }

    // Wait for HTTP thread
    if (m_httpThread.joinable()) {
        m_httpThread.join();
    }
}

void RestServer::setupRoutes() {
    // Enable CORS
    m_httpServer->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    // Handle OPTIONS for CORS preflight
    m_httpServer->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // Setup API routes
    ApiRouter::setupRoutes(*m_httpServer, m_mjpegStreamer.get());
}

void RestServer::runHttpServer() {
    std::cout << "[RestServer] Starting HTTP server on " << m_host << ":" << m_port << "\n";

    if (!m_httpServer->listen(m_host.c_str(), m_port)) {
        std::cerr << "[RestServer] Failed to start HTTP server\n";
        m_running.store(false);
    }
}

} // namespace crsdk_rest
