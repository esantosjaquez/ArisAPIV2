#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>
#include <json.hpp>

namespace crsdk_rest {

class SerialPort;

// GRBL settings descriptions
struct GrblSetting {
    int id;
    double value;
    std::string description;
};

// Position data
struct GrblPosition {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// GRBL status
struct GrblStatus {
    std::string state;           // Idle, Run, Hold, Jog, Alarm, Door, Check, Home, Sleep
    GrblPosition machinePos;
    GrblPosition workPos;
    double feedRate = 0;
    double spindleSpeed = 0;
    int feedOverride = 100;
    int rapidOverride = 100;
    int spindleOverride = 100;
    std::string inputPins;
    int bufferPlannerAvail = 0;
    int bufferRxAvail = 0;
};

class GrblController {
public:
    static GrblController& getInstance();

    // Connection
    std::vector<std::string> listPorts();
    bool connect(const std::string& port = "", int baudRate = 115200);
    void disconnect();
    bool isConnected() const { return m_connected.load(); }
    std::string getPort() const;
    std::string getVersion() const;

    // Status
    GrblStatus getStatus();
    nlohmann::json getStatusJson();
    std::string getState();

    // Movement
    bool home();
    bool moveG0(std::optional<double> x, std::optional<double> y, std::optional<double> z);
    bool moveG1(std::optional<double> x, std::optional<double> y, std::optional<double> z, double feed);
    bool jog(char axis, double distance, double feed);
    bool cancelJog();

    // Control
    bool feedHold();      // ! - Pause
    bool cycleStart();    // ~ - Resume
    bool softReset();     // 0x18 - Reset
    bool unlock();        // $X - Unlock after alarm

    // Settings
    std::vector<GrblSetting> getSettings();
    nlohmann::json getSettingsJson();
    bool setSetting(int id, double value);

    // Raw command
    std::string sendCommand(const std::string& cmd, int timeoutMs = 5000);
    bool sendRealTimeCommand(uint8_t cmd);

    // Event handler
    void setEventHandler(std::function<void(const std::string&, const nlohmann::json&)> handler);

private:
    GrblController() = default;
    ~GrblController();

    GrblController(const GrblController&) = delete;
    GrblController& operator=(const GrblController&) = delete;

    bool autoDetectPort(int baudRate);
    GrblStatus parseStatus(const std::string& response);
    std::vector<GrblSetting> parseSettings(const std::string& response);
    std::string waitForOk(int timeoutMs = 5000);
    void emitEvent(const std::string& type, const nlohmann::json& data = {});

    static std::string getSettingDescription(int id);

    std::unique_ptr<SerialPort> m_serial;
    std::string m_port;
    std::string m_version;
    std::atomic<bool> m_connected{false};
    mutable std::mutex m_mutex;
    std::function<void(const std::string&, const nlohmann::json&)> m_eventHandler;
};

} // namespace crsdk_rest
