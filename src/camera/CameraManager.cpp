#include "camera/CameraManager.h"
#include "CameraRemote_SDK.h"
#include <iostream>
#include <cstring>

namespace crsdk_rest {

namespace SDK = SCRSDK;

CameraManager& CameraManager::getInstance() {
    static CameraManager instance;
    return instance;
}

CameraManager::~CameraManager() {
    if (m_initialized.load()) {
        shutdown();
    }
}

bool CameraManager::initialize(uint32_t logType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized.load()) {
        return true;
    }

    bool result = SDK::Init(logType);
    if (result) {
        m_initialized.store(true);
        std::cout << "[CameraManager] SDK initialized successfully\n";
    } else {
        std::cerr << "[CameraManager] Failed to initialize SDK\n";
    }
    return result;
}

void CameraManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized.load()) {
        return;
    }

    // Disconnect all cameras first
    for (auto& pair : m_cameras) {
        if (pair.second && pair.second->isConnected()) {
            pair.second->disconnect();
        }
    }
    m_cameras.clear();
    m_cameraInfoList.clear();

    SDK::Release();
    m_initialized.store(false);
    std::cout << "[CameraManager] SDK shutdown complete\n";
}

std::vector<CameraInfo> CameraManager::enumerateCameras(uint8_t timeoutSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CameraInfo> result;

    if (!m_initialized.load()) {
        std::cerr << "[CameraManager] SDK not initialized\n";
        return result;
    }

    SDK::ICrEnumCameraObjectInfo* enumInfo = nullptr;
    auto err = SDK::EnumCameraObjects(&enumInfo, timeoutSec);

    if (err != SDK::CrError_None || !enumInfo) {
        std::cerr << "[CameraManager] EnumCameraObjects failed: 0x" << std::hex << err << std::dec << "\n";
        return result;
    }

    CrInt32u count = enumInfo->GetCount();
    std::cout << "[CameraManager] Found " << count << " camera(s)\n";

    m_cameraInfoList.clear();

    for (CrInt32u i = 0; i < count; i++) {
        auto* info = enumInfo->GetCameraObjectInfo(i);
        if (!info) continue;

        m_cameraInfoList.push_back(const_cast<void*>(static_cast<const void*>(info)));

        CameraInfo camInfo;
        camInfo.index = static_cast<int>(i);

        // Get model name
        auto model = info->GetModel();
        if (model) {
            // Convert from SDK char type to std::string
            std::string modelStr;
            const CrChar* p = model;
            while (*p) {
                modelStr += static_cast<char>(*p);
                p++;
            }
            camInfo.model = modelStr;
        }

        // Generate ID from index
        camInfo.id = "camera-" + std::to_string(i);

        // Get connection type
        auto connType = info->GetConnectionTypeName();
        if (connType) {
            std::string connStr;
            const CrChar* p = connType;
            while (*p) {
                connStr += static_cast<char>(*p);
                p++;
            }
            camInfo.connectionType = connStr;
        }

        camInfo.sshSupported = (info->GetSSHsupport() != 0);

        result.push_back(camInfo);
        std::cout << "[CameraManager] Camera " << i << ": " << camInfo.model
                  << " (" << camInfo.connectionType << ")\n";
    }

    return result;
}

std::shared_ptr<CameraDeviceWrapper> CameraManager::connectCamera(
    int cameraIndex, int mode, bool reconnect) {

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized.load()) {
        std::cerr << "[CameraManager] SDK not initialized\n";
        return nullptr;
    }

    // Check if already connected
    auto it = m_cameras.find(cameraIndex);
    if (it != m_cameras.end() && it->second && it->second->isConnected()) {
        std::cout << "[CameraManager] Camera " << cameraIndex << " already connected\n";
        return it->second;
    }

    // Get camera info
    if (cameraIndex < 0 || cameraIndex >= static_cast<int>(m_cameraInfoList.size())) {
        std::cerr << "[CameraManager] Invalid camera index: " << cameraIndex << "\n";
        return nullptr;
    }

    auto* info = static_cast<SDK::ICrCameraObjectInfo*>(m_cameraInfoList[cameraIndex]);
    if (!info) {
        std::cerr << "[CameraManager] Camera info not found for index: " << cameraIndex << "\n";
        return nullptr;
    }

    // Create wrapper
    auto wrapper = std::make_shared<CameraDeviceWrapper>(
        cameraIndex, info,
        [this](const CameraEvent& event) {
            dispatchEvent(event);
        }
    );

    // Connect
    if (!wrapper->connect(mode, reconnect)) {
        std::cerr << "[CameraManager] Failed to connect to camera " << cameraIndex << "\n";
        return nullptr;
    }

    m_cameras[cameraIndex] = wrapper;
    return wrapper;
}

void CameraManager::disconnectCamera(int cameraIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cameras.find(cameraIndex);
    if (it != m_cameras.end()) {
        if (it->second && it->second->isConnected()) {
            it->second->disconnect();
        }
        m_cameras.erase(it);
    }
}

void CameraManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& pair : m_cameras) {
        if (pair.second && pair.second->isConnected()) {
            pair.second->disconnect();
        }
    }
    m_cameras.clear();
}

std::shared_ptr<CameraDeviceWrapper> CameraManager::getConnectedCamera(int cameraIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cameras.find(cameraIndex);
    if (it != m_cameras.end() && it->second && it->second->isConnected()) {
        return it->second;
    }
    return nullptr;
}

std::vector<int> CameraManager::getConnectedCameraIndices() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<int> result;
    for (const auto& pair : m_cameras) {
        if (pair.second && pair.second->isConnected()) {
            result.push_back(pair.first);
        }
    }
    return result;
}

uint32_t CameraManager::getSDKVersion() {
    return SDK::GetSDKVersion();
}

uint32_t CameraManager::getSDKSerial() {
    return SDK::GetSDKSerial();
}

void CameraManager::setEventHandler(std::function<void(const CameraEvent&)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHandler = handler;
}

void CameraManager::dispatchEvent(const CameraEvent& event) {
    std::function<void(const CameraEvent&)> handler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handler = m_eventHandler;
    }
    if (handler) {
        handler(event);
    }
}

} // namespace crsdk_rest
