#include "grbl/SerialPort.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace crsdk_rest {

SerialPort::SerialPort() : m_fd(-1) {
}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& device, int baudRate) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd >= 0) {
        ::close(m_fd);
    }

    m_fd = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        std::cerr << "[SerialPort] Failed to open " << device << ": " << strerror(errno) << "\n";
        return false;
    }

    // Clear non-blocking for normal operation
    int flags = fcntl(m_fd, F_GETFL, 0);
    fcntl(m_fd, F_SETFL, flags & ~O_NONBLOCK);

    if (!configurePort(baudRate)) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    m_device = device;
    std::cout << "[SerialPort] Opened " << device << " at " << baudRate << " baud\n";
    return true;
}

bool SerialPort::configurePort(int baudRate) {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(m_fd, &tty) != 0) {
        std::cerr << "[SerialPort] tcgetattr failed: " << strerror(errno) << "\n";
        return false;
    }

    // Set baud rate
    speed_t speed;
    switch (baudRate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        default:     speed = B115200; break;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;  // No hardware flow control
    tty.c_cflag |= (CLOCAL | CREAD);  // Enable receiver, ignore modem controls

    // Raw input mode
    tty.c_lflag = 0;
    tty.c_iflag = 0;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // No software flow control
    tty.c_oflag = 0;

    // Read settings
    tty.c_cc[VMIN] = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 1;  // 100ms timeout

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        std::cerr << "[SerialPort] tcsetattr failed: " << strerror(errno) << "\n";
        return false;
    }

    // Flush buffers
    tcflush(m_fd, TCIOFLUSH);

    return true;
}

void SerialPort::close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
        std::cout << "[SerialPort] Closed " << m_device << "\n";
        m_device.clear();
    }
}

bool SerialPort::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fd >= 0;
}

bool SerialPort::write(const std::string& data) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd < 0) return false;

    std::string toSend = data;
    if (!toSend.empty() && toSend.back() != '\n') {
        toSend += '\n';
    }

    ssize_t written = ::write(m_fd, toSend.c_str(), toSend.size());
    if (written < 0) {
        std::cerr << "[SerialPort] Write failed: " << strerror(errno) << "\n";
        return false;
    }

    tcdrain(m_fd);  // Wait for transmission
    return written == static_cast<ssize_t>(toSend.size());
}

bool SerialPort::writeByte(uint8_t byte) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd < 0) return false;

    ssize_t written = ::write(m_fd, &byte, 1);
    return written == 1;
}

int SerialPort::waitForData(int timeoutMs) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_fd, &readfds);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    return select(m_fd + 1, &readfds, nullptr, nullptr, &tv);
}

std::string SerialPort::readLine(int timeoutMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd < 0) return "";

    std::string line;
    char c;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) break;

        int remaining = timeoutMs - static_cast<int>(elapsed);
        if (waitForData(remaining) <= 0) break;

        ssize_t n = ::read(m_fd, &c, 1);
        if (n <= 0) break;

        if (c == '\n') {
            // Remove trailing CR if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            break;
        }

        line += c;
    }

    return line;
}

std::string SerialPort::readAll(int timeoutMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd < 0) return "";

    std::string result;
    char buffer[256];

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) break;

        int remaining = timeoutMs - static_cast<int>(elapsed);
        if (waitForData(remaining) <= 0) break;

        ssize_t n = ::read(m_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) break;

        buffer[n] = '\0';
        result += buffer;
    }

    return result;
}

void SerialPort::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd >= 0) {
        tcflush(m_fd, TCIOFLUSH);
    }
}

void SerialPort::drain() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_fd >= 0) {
        tcdrain(m_fd);
    }
}

std::vector<std::string> SerialPort::listPorts() {
    std::vector<std::string> ports;

    DIR* dir = opendir("/dev");
    if (!dir) {
        return ports;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        // Look for ttyUSB* and ttyACM* (Arduino/GRBL devices)
        if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0) {
            ports.push_back("/dev/" + name);
        }
    }

    closedir(dir);

    std::sort(ports.begin(), ports.end());
    return ports;
}

} // namespace crsdk_rest
