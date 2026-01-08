#include "grbl/GrblController.h"
#include "grbl/SerialPort.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <regex>

namespace crsdk_rest {

GrblController& GrblController::getInstance() {
    static GrblController instance;
    return instance;
}

GrblController::~GrblController() {
    disconnect();
}

std::vector<std::string> GrblController::listPorts() {
    return SerialPort::listPorts();
}

bool GrblController::connect(const std::string& port, int baudRate) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_connected.load()) {
        std::cout << "[GRBL] Already connected\n";
        return true;
    }

    m_serial = std::make_unique<SerialPort>();

    // Auto-detect or use specified port
    std::string targetPort = port;
    if (targetPort.empty()) {
        // Release lock temporarily for auto-detect
        m_mutex.unlock();
        bool found = autoDetectPort(baudRate);
        m_mutex.lock();

        if (!found) {
            std::cerr << "[GRBL] No GRBL device found\n";
            m_serial.reset();
            return false;
        }
        return true;  // autoDetectPort sets m_connected
    }

    // Connect to specified port
    if (!m_serial->open(targetPort, baudRate)) {
        std::cerr << "[GRBL] Failed to open " << targetPort << "\n";
        m_serial.reset();
        return false;
    }

    m_port = targetPort;

    // Send soft reset and wait for GRBL banner
    m_serial->writeByte(0x18);  // Ctrl+X soft reset
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Read banner
    std::string response = m_serial->readAll(1000);

    // Look for "Grbl" in response
    if (response.find("Grbl") != std::string::npos) {
        // Extract version line
        std::istringstream iss(response);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Grbl") != std::string::npos) {
                m_version = line;
                break;
            }
        }

        m_connected.store(true);
        std::cout << "[GRBL] Connected to " << m_port << ": " << m_version << "\n";

        emitEvent("grbl_connected", {
            {"port", m_port},
            {"version", m_version}
        });

        return true;
    }

    std::cerr << "[GRBL] No GRBL response from " << targetPort << "\n";
    m_serial->close();
    m_serial.reset();
    return false;
}

bool GrblController::autoDetectPort(int baudRate) {
    auto ports = SerialPort::listPorts();

    std::cout << "[GRBL] Auto-detecting... found " << ports.size() << " serial port(s)\n";

    for (const auto& port : ports) {
        std::cout << "[GRBL] Trying " << port << "...\n";

        SerialPort serial;
        if (!serial.open(port, baudRate)) {
            continue;
        }

        // Send soft reset
        serial.writeByte(0x18);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Read response
        std::string response = serial.readAll(1000);

        if (response.find("Grbl") != std::string::npos) {
            serial.close();

            // Now open with our member serial
            std::lock_guard<std::mutex> lock(m_mutex);

            m_serial = std::make_unique<SerialPort>();
            if (!m_serial->open(port, baudRate)) {
                m_serial.reset();
                continue;
            }

            m_port = port;

            // Extract version
            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("Grbl") != std::string::npos) {
                    m_version = line;
                    break;
                }
            }

            m_connected.store(true);
            std::cout << "[GRBL] Found GRBL on " << m_port << ": " << m_version << "\n";

            emitEvent("grbl_connected", {
                {"port", m_port},
                {"version", m_version}
            });

            return true;
        }

        serial.close();
    }

    return false;
}

void GrblController::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_serial && m_connected.load()) {
        m_serial->close();
        m_serial.reset();
        m_connected.store(false);

        std::cout << "[GRBL] Disconnected from " << m_port << "\n";

        emitEvent("grbl_disconnected", {{"port", m_port}});

        m_port.clear();
        m_version.clear();
    }
}

std::string GrblController::getPort() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_port;
}

std::string GrblController::getVersion() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_version;
}

GrblStatus GrblController::getStatus() {
    std::lock_guard<std::mutex> lock(m_mutex);

    GrblStatus status;
    status.state = "Unknown";

    if (!m_connected.load() || !m_serial) {
        status.state = "Disconnected";
        return status;
    }

    // Send status query
    m_serial->writeByte('?');

    // Read response
    std::string response = m_serial->readLine(500);

    if (!response.empty() && response[0] == '<') {
        return parseStatus(response);
    }

    return status;
}

nlohmann::json GrblController::getStatusJson() {
    GrblStatus status = getStatus();

    nlohmann::json json;
    json["state"] = status.state;
    json["machinePosition"] = {
        {"x", status.machinePos.x},
        {"y", status.machinePos.y},
        {"z", status.machinePos.z}
    };
    json["workPosition"] = {
        {"x", status.workPos.x},
        {"y", status.workPos.y},
        {"z", status.workPos.z}
    };
    json["feed"] = status.feedRate;
    json["spindle"] = status.spindleSpeed;
    json["override"] = {
        {"feed", status.feedOverride},
        {"rapid", status.rapidOverride},
        {"spindle", status.spindleOverride}
    };
    json["inputPins"] = status.inputPins;
    json["buffer"] = {
        {"planner", status.bufferPlannerAvail},
        {"rx", status.bufferRxAvail}
    };

    return json;
}

std::string GrblController::getState() {
    return getStatus().state;
}

GrblStatus GrblController::parseStatus(const std::string& response) {
    // Format: <State|MPos:x,y,z|WPos:x,y,z|Bf:p,r|FS:f,s|Ov:f,r,s|Pn:XYZ>
    GrblStatus status;

    // Extract state
    size_t stateEnd = response.find('|');
    if (stateEnd != std::string::npos && response.size() > 1) {
        status.state = response.substr(1, stateEnd - 1);
    }

    // Parse MPos
    size_t mposStart = response.find("MPos:");
    if (mposStart != std::string::npos) {
        mposStart += 5;
        size_t mposEnd = response.find('|', mposStart);
        std::string mpos = response.substr(mposStart, mposEnd - mposStart);

        std::istringstream iss(mpos);
        char comma;
        iss >> status.machinePos.x >> comma >> status.machinePos.y >> comma >> status.machinePos.z;
    }

    // Parse WPos
    size_t wposStart = response.find("WPos:");
    if (wposStart != std::string::npos) {
        wposStart += 5;
        size_t wposEnd = response.find('|', wposStart);
        if (wposEnd == std::string::npos) wposEnd = response.find('>', wposStart);
        std::string wpos = response.substr(wposStart, wposEnd - wposStart);

        std::istringstream iss(wpos);
        char comma;
        iss >> status.workPos.x >> comma >> status.workPos.y >> comma >> status.workPos.z;
    }

    // Parse Bf (buffer)
    size_t bfStart = response.find("Bf:");
    if (bfStart != std::string::npos) {
        bfStart += 3;
        int p, r;
        if (sscanf(response.c_str() + bfStart, "%d,%d", &p, &r) == 2) {
            status.bufferPlannerAvail = p;
            status.bufferRxAvail = r;
        }
    }

    // Parse FS (feed/spindle)
    size_t fsStart = response.find("FS:");
    if (fsStart != std::string::npos) {
        fsStart += 3;
        double f, s;
        if (sscanf(response.c_str() + fsStart, "%lf,%lf", &f, &s) == 2) {
            status.feedRate = f;
            status.spindleSpeed = s;
        }
    }

    // Parse F (feed only, older format)
    if (status.feedRate == 0) {
        size_t fStart = response.find("|F:");
        if (fStart != std::string::npos) {
            fStart += 3;
            sscanf(response.c_str() + fStart, "%lf", &status.feedRate);
        }
    }

    // Parse Ov (overrides)
    size_t ovStart = response.find("Ov:");
    if (ovStart != std::string::npos) {
        ovStart += 3;
        int f, r, s;
        if (sscanf(response.c_str() + ovStart, "%d,%d,%d", &f, &r, &s) == 3) {
            status.feedOverride = f;
            status.rapidOverride = r;
            status.spindleOverride = s;
        }
    }

    // Parse Pn (input pins)
    size_t pnStart = response.find("Pn:");
    if (pnStart != std::string::npos) {
        pnStart += 3;
        size_t pnEnd = response.find_first_of("|>", pnStart);
        status.inputPins = response.substr(pnStart, pnEnd - pnStart);
    }

    return status;
}

bool GrblController::home() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    m_serial->write("$H");
    std::string response = waitForOk(30000);  // Homing can take time

    bool success = (response.find("ok") != std::string::npos);

    if (success) {
        emitEvent("grbl_homing_complete", {});
    }

    return success;
}

bool GrblController::moveG0(std::optional<double> x, std::optional<double> y, std::optional<double> z) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    std::ostringstream cmd;
    cmd << "G0";
    cmd << std::fixed << std::setprecision(3);

    if (x.has_value()) cmd << " X" << x.value();
    if (y.has_value()) cmd << " Y" << y.value();
    if (z.has_value()) cmd << " Z" << z.value();

    m_serial->write(cmd.str());
    std::string response = waitForOk(5000);

    return response.find("ok") != std::string::npos;
}

bool GrblController::moveG1(std::optional<double> x, std::optional<double> y, std::optional<double> z, double feed) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    std::ostringstream cmd;
    cmd << "G1";
    cmd << std::fixed << std::setprecision(3);

    if (x.has_value()) cmd << " X" << x.value();
    if (y.has_value()) cmd << " Y" << y.value();
    if (z.has_value()) cmd << " Z" << z.value();
    cmd << " F" << feed;

    m_serial->write(cmd.str());
    std::string response = waitForOk(5000);

    return response.find("ok") != std::string::npos;
}

bool GrblController::jog(char axis, double distance, double feed) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    // Jog command format: $J=G91 X10 F1000
    std::ostringstream cmd;
    cmd << "$J=G91 " << axis;
    cmd << std::fixed << std::setprecision(3) << distance;
    cmd << " F" << feed;

    m_serial->write(cmd.str());
    std::string response = waitForOk(5000);

    return response.find("ok") != std::string::npos;
}

bool GrblController::cancelJog() {
    return sendRealTimeCommand(0x85);
}

bool GrblController::feedHold() {
    bool result = sendRealTimeCommand('!');
    if (result) {
        emitEvent("grbl_feed_hold", {});
    }
    return result;
}

bool GrblController::cycleStart() {
    bool result = sendRealTimeCommand('~');
    if (result) {
        emitEvent("grbl_cycle_start", {});
    }
    return result;
}

bool GrblController::softReset() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    m_serial->writeByte(0x18);  // Ctrl+X

    // Wait for GRBL to restart
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Read any response
    m_serial->readAll(500);

    emitEvent("grbl_reset", {});

    return true;
}

bool GrblController::unlock() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    m_serial->write("$X");
    std::string response = waitForOk(2000);

    bool success = response.find("ok") != std::string::npos;

    if (success) {
        emitEvent("grbl_unlocked", {});
    }

    return success;
}

std::vector<GrblSetting> GrblController::getSettings() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<GrblSetting> settings;

    if (!m_connected.load() || !m_serial) return settings;

    m_serial->write("$$");

    // Read all settings (multiple lines)
    std::string allResponse;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        std::string line = m_serial->readLine(500);
        if (line.empty()) break;

        allResponse += line + "\n";

        // Check for ok or error
        if (line.find("ok") != std::string::npos) break;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > 5000) break;
    }

    return parseSettings(allResponse);
}

nlohmann::json GrblController::getSettingsJson() {
    auto settings = getSettings();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : settings) {
        arr.push_back({
            {"id", s.id},
            {"value", s.value},
            {"description", s.description}
        });
    }

    return arr;
}

std::vector<GrblSetting> GrblController::parseSettings(const std::string& response) {
    std::vector<GrblSetting> settings;

    std::istringstream iss(response);
    std::string line;

    // Format: $0=10 or $100=250.000
    std::regex settingRegex(R"(\$(\d+)=([0-9.]+))");

    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, settingRegex)) {
            GrblSetting s;
            s.id = std::stoi(match[1]);
            s.value = std::stod(match[2]);
            s.description = getSettingDescription(s.id);
            settings.push_back(s);
        }
    }

    return settings;
}

bool GrblController::setSetting(int id, double value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    std::ostringstream cmd;
    cmd << "$" << id << "=" << std::fixed << std::setprecision(3) << value;

    m_serial->write(cmd.str());
    std::string response = waitForOk(2000);

    bool success = response.find("ok") != std::string::npos;

    if (success) {
        emitEvent("grbl_setting_changed", {
            {"id", id},
            {"value", value}
        });
    }

    return success;
}

std::string GrblController::sendCommand(const std::string& cmd, int timeoutMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return "error: not connected";

    m_serial->write(cmd);
    return waitForOk(timeoutMs);
}

bool GrblController::sendRealTimeCommand(uint8_t cmd) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || !m_serial) return false;

    return m_serial->writeByte(cmd);
}

std::string GrblController::waitForOk(int timeoutMs) {
    // Note: caller must hold mutex

    std::string result;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        std::string line = m_serial->readLine(500);

        if (!line.empty()) {
            result += line + "\n";

            if (line.find("ok") != std::string::npos ||
                line.find("error") != std::string::npos ||
                line.find("ALARM") != std::string::npos) {
                break;
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) {
            result += "timeout";
            break;
        }
    }

    return result;
}

void GrblController::setEventHandler(std::function<void(const std::string&, const nlohmann::json&)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHandler = handler;
}

void GrblController::emitEvent(const std::string& type, const nlohmann::json& data) {
    std::function<void(const std::string&, const nlohmann::json&)> handler;
    {
        // Don't hold lock while calling handler
        handler = m_eventHandler;
    }

    if (handler) {
        handler(type, data);
    }
}

std::string GrblController::getSettingDescription(int id) {
    static const std::map<int, std::string> descriptions = {
        {0, "Step pulse time (microseconds)"},
        {1, "Step idle delay (milliseconds)"},
        {2, "Step pulse invert mask"},
        {3, "Step direction invert mask"},
        {4, "Invert step enable pin"},
        {5, "Invert limit pins"},
        {6, "Invert probe pin"},
        {10, "Status report options"},
        {11, "Junction deviation (mm)"},
        {12, "Arc tolerance (mm)"},
        {13, "Report in inches"},
        {20, "Soft limits enable"},
        {21, "Hard limits enable"},
        {22, "Homing cycle enable"},
        {23, "Homing direction invert mask"},
        {24, "Homing locate feed rate (mm/min)"},
        {25, "Homing search seek rate (mm/min)"},
        {26, "Homing switch debounce delay (ms)"},
        {27, "Homing switch pull-off distance (mm)"},
        {30, "Maximum spindle speed (RPM)"},
        {31, "Minimum spindle speed (RPM)"},
        {32, "Laser mode enable"},
        {100, "X-axis steps per millimeter"},
        {101, "Y-axis steps per millimeter"},
        {102, "Z-axis steps per millimeter"},
        {110, "X-axis maximum rate (mm/min)"},
        {111, "Y-axis maximum rate (mm/min)"},
        {112, "Z-axis maximum rate (mm/min)"},
        {120, "X-axis acceleration (mm/sec^2)"},
        {121, "Y-axis acceleration (mm/sec^2)"},
        {122, "Z-axis acceleration (mm/sec^2)"},
        {130, "X-axis maximum travel (mm)"},
        {131, "Y-axis maximum travel (mm)"},
        {132, "Z-axis maximum travel (mm)"}
    };

    auto it = descriptions.find(id);
    if (it != descriptions.end()) {
        return it->second;
    }

    return "Unknown setting";
}

} // namespace crsdk_rest
