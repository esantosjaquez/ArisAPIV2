#include <iostream>
#include <csignal>
#include <atomic>
#include "server/RestServer.h"
#include "server/WebSocketHandler.h"
#include "camera/CameraManager.h"

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    std::cout << "=== Sony CrSDK REST API Server ===\n\n";

    // Parse command line args
    std::string host = "0.0.0.0";
    int port = 8080;
    int wsPort = 8081;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--ws-port" && i + 1 < argc) {
            wsPort = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --host <addr>     Bind address (default: 0.0.0.0)\n"
                      << "  --port <port>     HTTP port (default: 8080)\n"
                      << "  --ws-port <port>  WebSocket port (default: 8081)\n"
                      << "  --help, -h        Show this help\n";
            return 0;
        }
    }

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize SDK
    auto& manager = crsdk_rest::CameraManager::getInstance();
    if (!manager.initialize()) {
        std::cerr << "Failed to initialize Sony SDK\n";
        return 1;
    }

    std::cout << "SDK Version: 0x" << std::hex << manager.getSDKVersion() << std::dec << "\n";

    // Create and start server
    crsdk_rest::RestServer server(host, port, wsPort);

    // Set up event handler to broadcast to WebSocket clients
    manager.setEventHandler([&server](const crsdk_rest::CameraEvent& event) {
        if (server.getWebSocketHandler()) {
            server.getWebSocketHandler()->broadcast(event);
        }
    });

    if (!server.start()) {
        std::cerr << "Failed to start server\n";
        manager.shutdown();
        return 1;
    }

    std::cout << "\nServer running on http://" << host << ":" << port << "\n";
    std::cout << "WebSocket on ws://" << host << ":" << wsPort << "/events\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    // Main loop
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down...\n";

    server.stop();
    manager.disconnectAll();
    manager.shutdown();

    std::cout << "Goodbye!\n";
    return 0;
}
