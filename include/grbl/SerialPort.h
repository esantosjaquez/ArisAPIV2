#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace crsdk_rest {

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    // Prevent copying
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Connection
    bool open(const std::string& device, int baudRate = 115200);
    void close();
    bool isOpen() const;
    std::string getDevice() const { return m_device; }

    // I/O
    bool write(const std::string& data);
    bool writeByte(uint8_t byte);
    std::string readLine(int timeoutMs = 1000);
    std::string readAll(int timeoutMs = 100);
    void flush();
    void drain();

    // Static utilities
    static std::vector<std::string> listPorts();

private:
    int m_fd = -1;
    std::string m_device;
    mutable std::mutex m_mutex;

    bool configurePort(int baudRate);
    int waitForData(int timeoutMs);
};

} // namespace crsdk_rest
