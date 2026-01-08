#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>

// Ensure SSL support is disabled in httplib
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

namespace crsdk_rest {

class WebSocketHandler;
class MjpegStreamer;

class RestServer {
public:
    RestServer(const std::string& host = "0.0.0.0", int port = 8080, int wsPort = 8081);
    ~RestServer();

    bool start();
    void stop();
    bool isRunning() const { return m_running.load(); }

    WebSocketHandler* getWebSocketHandler() { return m_wsHandler.get(); }

private:
    void setupRoutes();
    void runHttpServer();

    std::string m_host;
    int m_port;
    int m_wsPort;
    std::atomic<bool> m_running{false};

    std::unique_ptr<httplib::Server> m_httpServer;
    std::unique_ptr<WebSocketHandler> m_wsHandler;
    std::unique_ptr<MjpegStreamer> m_mjpegStreamer;
    std::thread m_httpThread;
};

} // namespace crsdk_rest
