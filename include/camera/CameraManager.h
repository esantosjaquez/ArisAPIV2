#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include "CameraDeviceWrapper.h"

namespace crsdk_rest {

struct CameraInfo {
    int index;
    std::string id;
    std::string model;
    std::string connectionType;
    bool sshSupported;
};

class CameraManager {
public:
    static CameraManager& getInstance();

    // Lifecycle
    bool initialize(uint32_t logType = 0);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // Discovery
    std::vector<CameraInfo> enumerateCameras(uint8_t timeoutSec = 3);

    // Connection management
    std::shared_ptr<CameraDeviceWrapper> connectCamera(
        int cameraIndex,
        int mode = 0,  // 0=Remote, 1=ContentsTransfer
        bool reconnect = true
    );
    void disconnectCamera(int cameraIndex);
    void disconnectAll();
    std::shared_ptr<CameraDeviceWrapper> getConnectedCamera(int cameraIndex);
    std::vector<int> getConnectedCameraIndices();

    // SDK info
    uint32_t getSDKVersion();
    uint32_t getSDKSerial();

    // Event callback
    void setEventHandler(std::function<void(const CameraEvent&)> handler);
    void dispatchEvent(const CameraEvent& event);

private:
    CameraManager() = default;
    ~CameraManager();
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;
    std::unordered_map<int, std::shared_ptr<CameraDeviceWrapper>> m_cameras;
    std::vector<void*> m_cameraInfoList;  // Store ICrCameraObjectInfo pointers
    std::function<void(const CameraEvent&)> m_eventHandler;
};

} // namespace crsdk_rest
