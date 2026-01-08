#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include "CameraRemote_SDK.h"
#include "IDeviceCallback.h"
#include <json.hpp>

namespace crsdk_rest {

struct CameraEvent {
    std::string type;
    int cameraIndex;
    nlohmann::json data;
    std::chrono::system_clock::time_point timestamp;

    CameraEvent() : cameraIndex(-1), timestamp(std::chrono::system_clock::now()) {}
    CameraEvent(const std::string& t, int idx)
        : type(t), cameraIndex(idx), timestamp(std::chrono::system_clock::now()) {}
};

class CameraDeviceWrapper : public SCRSDK::IDeviceCallback {
public:
    CameraDeviceWrapper(int index, SCRSDK::ICrCameraObjectInfo* info,
                        std::function<void(const CameraEvent&)> eventCallback);
    ~CameraDeviceWrapper();

    // Connection
    bool connect(int mode, bool reconnect);
    bool disconnect();
    bool isConnected() const { return m_connected.load(); }
    int getIndex() const { return m_index; }
    std::string getModel() const { return m_model; }

    // Properties
    nlohmann::json getAllProperties();
    nlohmann::json getSelectProperties(const std::vector<uint32_t>& codes);
    bool setProperty(uint32_t code, uint64_t value);

    // Commands
    bool sendCommand(uint32_t commandId, uint32_t param);
    bool capture();
    bool startRecording();
    bool stopRecording();
    bool halfPressShutter();
    bool releaseShutter();

    // Live View
    std::vector<uint8_t> getLiveViewImage();
    nlohmann::json getLiveViewInfo();

    // Content Transfer
    nlohmann::json getDateFolderList();
    nlohmann::json getContentsHandleList(uint32_t folderHandle);
    nlohmann::json getContentsDetailInfo(uint32_t contentHandle);
    bool pullContentsFile(uint32_t contentHandle, const std::string& savePath);
    std::vector<uint8_t> getThumbnail(uint32_t contentHandle);

    // IDeviceCallback implementations
    void OnConnected(SCRSDK::DeviceConnectionVersioin version) override;
    void OnDisconnected(CrInt32u error) override;
    void OnPropertyChanged() override;
    void OnPropertyChangedCodes(CrInt32u num, CrInt32u* codes) override;
    void OnLvPropertyChanged() override;
    void OnLvPropertyChangedCodes(CrInt32u num, CrInt32u* codes) override;
    void OnCompleteDownload(CrChar* filename, CrInt32u type) override;
    void OnNotifyContentsTransfer(CrInt32u notify, SCRSDK::CrContentHandle handle, CrChar* filename) override;
    void OnWarning(CrInt32u warning) override;
    void OnWarningExt(CrInt32u warning, CrInt32 param1, CrInt32 param2, CrInt32 param3) override;
    void OnError(CrInt32u error) override;

private:
    void emitEvent(const std::string& type, const nlohmann::json& data = {});

    int m_index;
    SCRSDK::ICrCameraObjectInfo* m_info;
    SCRSDK::CrDeviceHandle m_handle{0};
    std::atomic<bool> m_connected{false};
    int m_mode{0};
    std::string m_model;
    std::function<void(const CameraEvent&)> m_eventCallback;

    mutable std::mutex m_mutex;
    mutable std::mutex m_liveViewMutex;
    std::vector<uint8_t> m_liveViewBuffer;
};

} // namespace crsdk_rest
