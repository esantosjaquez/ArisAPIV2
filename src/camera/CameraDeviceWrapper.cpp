#include "camera/CameraDeviceWrapper.h"
#include "CameraRemote_SDK.h"
#include "CrDeviceProperty.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace crsdk_rest {

namespace SDK = SCRSDK;

CameraDeviceWrapper::CameraDeviceWrapper(
    int index,
    SDK::ICrCameraObjectInfo* info,
    std::function<void(const CameraEvent&)> eventCallback)
    : m_index(index)
    , m_info(info)
    , m_eventCallback(eventCallback)
{
    // Get model name
    if (info) {
        auto model = info->GetModel();
        if (model) {
            const CrChar* p = model;
            while (*p) {
                m_model += static_cast<char>(*p);
                p++;
            }
        }
    }
}

CameraDeviceWrapper::~CameraDeviceWrapper() {
    if (m_connected.load()) {
        disconnect();
    }
}

bool CameraDeviceWrapper::connect(int mode, bool reconnect) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_connected.load()) {
        return true;
    }

    SDK::CrSdkControlMode sdkMode = (mode == 0)
        ? SDK::CrSdkControlMode_Remote
        : SDK::CrSdkControlMode_ContentsTransfer;

    SDK::CrReconnectingSet recon = reconnect
        ? SDK::CrReconnecting_ON
        : SDK::CrReconnecting_OFF;

    std::cout << "[Camera " << m_index << "] Connecting in "
              << (mode == 0 ? "Remote" : "ContentsTransfer") << " mode...\n";

    auto err = SDK::Connect(m_info, this, &m_handle, sdkMode, recon);

    if (err != SDK::CrError_None) {
        std::cerr << "[Camera " << m_index << "] Connect failed: 0x"
                  << std::hex << err << std::dec << "\n";
        return false;
    }

    m_mode = mode;

    // Wait briefly for OnConnected callback
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return m_connected.load();
}

bool CameraDeviceWrapper::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || m_handle == 0) {
        return true;
    }

    std::cout << "[Camera " << m_index << "] Disconnecting...\n";

    auto err = SDK::Disconnect(m_handle);
    if (err != SDK::CrError_None) {
        std::cerr << "[Camera " << m_index << "] Disconnect failed: 0x"
                  << std::hex << err << std::dec << "\n";
    }

    SDK::ReleaseDevice(m_handle);
    m_handle = 0;
    m_connected.store(false);

    return true;
}

nlohmann::json CameraDeviceWrapper::getAllProperties() {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json result = nlohmann::json::array();

    if (!m_connected.load() || m_handle == 0) {
        return result;
    }

    SDK::CrDeviceProperty* propList = nullptr;
    CrInt32 numProps = 0;

    auto err = SDK::GetDeviceProperties(m_handle, &propList, &numProps);
    if (err != SDK::CrError_None || !propList) {
        return result;
    }

    for (CrInt32 i = 0; i < numProps; i++) {
        auto& prop = propList[i];
        nlohmann::json propJson;
        propJson["code"] = prop.GetCode();
        propJson["currentValue"] = prop.GetCurrentValue();
        propJson["writable"] = prop.IsSetEnableCurrentValue();

        // Get possible values
        auto valueSize = prop.GetValueSize();
        if (valueSize > 0) {
            nlohmann::json possibleValues = nlohmann::json::array();
            auto values = prop.GetValues();

            // Determine value type and size
            switch (prop.GetValueType()) {
                case SDK::CrDataType_UInt8:
                case SDK::CrDataType_UInt8Array: {
                    int count = valueSize / sizeof(uint8_t);
                    auto* arr = reinterpret_cast<const uint8_t*>(values);
                    for (int j = 0; j < count; j++) {
                        possibleValues.push_back(arr[j]);
                    }
                    break;
                }
                case SDK::CrDataType_UInt16:
                case SDK::CrDataType_UInt16Array: {
                    int count = valueSize / sizeof(uint16_t);
                    auto* arr = reinterpret_cast<const uint16_t*>(values);
                    for (int j = 0; j < count; j++) {
                        possibleValues.push_back(arr[j]);
                    }
                    break;
                }
                case SDK::CrDataType_UInt32:
                case SDK::CrDataType_UInt32Array: {
                    int count = valueSize / sizeof(uint32_t);
                    auto* arr = reinterpret_cast<const uint32_t*>(values);
                    for (int j = 0; j < count; j++) {
                        possibleValues.push_back(arr[j]);
                    }
                    break;
                }
                case SDK::CrDataType_UInt64:
                case SDK::CrDataType_UInt64Array: {
                    int count = valueSize / sizeof(uint64_t);
                    auto* arr = reinterpret_cast<const uint64_t*>(values);
                    for (int j = 0; j < count; j++) {
                        possibleValues.push_back(arr[j]);
                    }
                    break;
                }
                default:
                    break;
            }
            propJson["possibleValues"] = possibleValues;
        }

        result.push_back(propJson);
    }

    SDK::ReleaseDeviceProperties(m_handle, propList);
    return result;
}

nlohmann::json CameraDeviceWrapper::getSelectProperties(const std::vector<uint32_t>& codes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json result = nlohmann::json::array();

    if (!m_connected.load() || m_handle == 0 || codes.empty()) {
        return result;
    }

    SDK::CrDeviceProperty* propList = nullptr;
    CrInt32 numProps = 0;

    auto err = SDK::GetSelectDeviceProperties(
        m_handle,
        static_cast<CrInt32u>(codes.size()),
        const_cast<CrInt32u*>(codes.data()),
        &propList,
        &numProps
    );

    if (err != SDK::CrError_None || !propList) {
        return result;
    }

    for (CrInt32 i = 0; i < numProps; i++) {
        auto& prop = propList[i];
        nlohmann::json propJson;
        propJson["code"] = prop.GetCode();
        propJson["currentValue"] = prop.GetCurrentValue();
        propJson["writable"] = prop.IsSetEnableCurrentValue();
        result.push_back(propJson);
    }

    SDK::ReleaseDeviceProperties(m_handle, propList);
    return result;
}

bool CameraDeviceWrapper::setProperty(uint32_t code, uint64_t value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || m_handle == 0) {
        return false;
    }

    SDK::CrDeviceProperty prop;
    prop.SetCode(code);
    prop.SetCurrentValue(value);
    prop.SetValueType(SDK::CrDataType_UInt32Array);

    auto err = SDK::SetDeviceProperty(m_handle, &prop);
    if (err != SDK::CrError_None) {
        std::cerr << "[Camera " << m_index << "] SetDeviceProperty failed: 0x"
                  << std::hex << err << std::dec << "\n";
        return false;
    }

    return true;
}

bool CameraDeviceWrapper::sendCommand(uint32_t commandId, uint32_t param) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || m_handle == 0) {
        return false;
    }

    auto err = SDK::SendCommand(m_handle, commandId, static_cast<SDK::CrCommandParam>(param));
    if (err != SDK::CrError_None) {
        std::cerr << "[Camera " << m_index << "] SendCommand failed: 0x"
                  << std::hex << err << std::dec << "\n";
        return false;
    }

    return true;
}

bool CameraDeviceWrapper::capture() {
    // Press shutter
    if (!sendCommand(SDK::CrCommandId_Release, SDK::CrCommandParam_Down)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    // Release shutter
    return sendCommand(SDK::CrCommandId_Release, SDK::CrCommandParam_Up);
}

bool CameraDeviceWrapper::startRecording() {
    return sendCommand(SDK::CrCommandId_MovieRecord, SDK::CrCommandParam_Down);
}

bool CameraDeviceWrapper::stopRecording() {
    return sendCommand(SDK::CrCommandId_MovieRecord, SDK::CrCommandParam_Up);
}

bool CameraDeviceWrapper::halfPressShutter() {
    return sendCommand(SDK::CrCommandId_Release, SDK::CrCommandParam_Down);
}

bool CameraDeviceWrapper::releaseShutter() {
    return sendCommand(SDK::CrCommandId_Release, SDK::CrCommandParam_Up);
}

std::vector<uint8_t> CameraDeviceWrapper::getLiveViewImage() {
    std::lock_guard<std::mutex> lock(m_liveViewMutex);

    if (!m_connected.load() || m_handle == 0) {
        return {};
    }

    // Get buffer size
    SDK::CrImageInfo info;
    auto err = SDK::GetLiveViewImageInfo(m_handle, &info);
    if (err != SDK::CrError_None) {
        return {};
    }

    uint32_t bufSize = info.GetBufferSize();
    if (bufSize < 1) {
        return {};
    }

    // Resize buffer if needed
    if (m_liveViewBuffer.size() < bufSize) {
        m_liveViewBuffer.resize(bufSize);
    }

    SDK::CrImageDataBlock imageData;
    imageData.SetSize(bufSize);
    imageData.SetData(m_liveViewBuffer.data());

    err = SDK::GetLiveViewImage(m_handle, &imageData);
    if (err != SDK::CrError_None) {
        return {};
    }

    // Return just the image data
    auto* imgPtr = imageData.GetImageData();
    auto imgSize = imageData.GetImageSize();

    if (!imgPtr || imgSize == 0) {
        return {};
    }

    return std::vector<uint8_t>(imgPtr, imgPtr + imgSize);
}

nlohmann::json CameraDeviceWrapper::getLiveViewInfo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json result;

    if (!m_connected.load() || m_handle == 0) {
        return result;
    }

    SDK::CrImageInfo info;
    auto err = SDK::GetLiveViewImageInfo(m_handle, &info);
    if (err == SDK::CrError_None) {
        result["bufferSize"] = info.GetBufferSize();
    }

    return result;
}

nlohmann::json CameraDeviceWrapper::getDateFolderList() {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json result = nlohmann::json::array();

    if (!m_connected.load() || m_handle == 0) {
        return result;
    }

    SDK::CrMtpFolderInfo* folders = nullptr;
    CrInt32u numFolders = 0;

    auto err = SDK::GetDateFolderList(m_handle, &folders, &numFolders);
    if (err != SDK::CrError_None || !folders) {
        return result;
    }

    for (CrInt32u i = 0; i < numFolders; i++) {
        nlohmann::json folder;
        folder["handle"] = folders[i].handle;
        // Convert folder name
        if (folders[i].folderName && folders[i].folderNameSize > 0) {
            std::string name;
            const CrChar* p = folders[i].folderName;
            while (*p) {
                name += static_cast<char>(*p);
                p++;
            }
            folder["name"] = name;
        }
        result.push_back(folder);
    }

    SDK::ReleaseDateFolderList(m_handle, folders);
    return result;
}

nlohmann::json CameraDeviceWrapper::getContentsHandleList(uint32_t folderHandle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json result = nlohmann::json::array();

    if (!m_connected.load() || m_handle == 0) {
        return result;
    }

    SDK::CrContentHandle* handles = nullptr;
    CrInt32u numContents = 0;

    auto err = SDK::GetContentsHandleList(m_handle, folderHandle, &handles, &numContents);
    if (err != SDK::CrError_None || !handles) {
        return result;
    }

    for (CrInt32u i = 0; i < numContents; i++) {
        result.push_back(handles[i]);
    }

    SDK::ReleaseContentsHandleList(m_handle, handles);
    return result;
}

nlohmann::json CameraDeviceWrapper::getContentsDetailInfo(uint32_t contentHandle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json result;

    if (!m_connected.load() || m_handle == 0) {
        return result;
    }

    SDK::CrMtpContentsInfo info;
    auto err = SDK::GetContentsDetailInfo(m_handle, contentHandle, &info);
    if (err != SDK::CrError_None) {
        return result;
    }

    result["handle"] = contentHandle;
    // Add more fields based on CrMtpContentsInfo structure

    return result;
}

bool CameraDeviceWrapper::pullContentsFile(uint32_t contentHandle, const std::string& savePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || m_handle == 0) {
        return false;
    }

    // Convert path to SDK format
    std::vector<CrChar> pathBuf(savePath.begin(), savePath.end());
    pathBuf.push_back(0);

    auto err = SDK::PullContentsFile(m_handle, contentHandle,
                                     SDK::CrPropertyStillImageTransSize_Original,
                                     pathBuf.data(), nullptr);

    return err == SDK::CrError_None;
}

std::vector<uint8_t> CameraDeviceWrapper::getThumbnail(uint32_t contentHandle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected.load() || m_handle == 0) {
        return {};
    }

    std::vector<uint8_t> buffer(64 * 1024);  // 64KB buffer
    SDK::CrImageDataBlock imageData;
    imageData.SetSize(static_cast<CrInt32u>(buffer.size()));
    imageData.SetData(buffer.data());

    SDK::CrFileType fileType;
    auto err = SDK::GetContentsThumbnailImage(m_handle, contentHandle, &imageData, &fileType);

    if (err != SDK::CrError_None) {
        return {};
    }

    auto* imgPtr = imageData.GetImageData();
    auto imgSize = imageData.GetImageSize();

    if (!imgPtr || imgSize == 0) {
        return {};
    }

    return std::vector<uint8_t>(imgPtr, imgPtr + imgSize);
}

// IDeviceCallback implementations
void CameraDeviceWrapper::OnConnected(SDK::DeviceConnectionVersioin version) {
    m_connected.store(true);
    std::cout << "[Camera " << m_index << "] Connected (version: " << version << ")\n";
    emitEvent("connected", {{"version", static_cast<int>(version)}});
}

void CameraDeviceWrapper::OnDisconnected(CrInt32u error) {
    m_connected.store(false);
    std::cout << "[Camera " << m_index << "] Disconnected (error: 0x"
              << std::hex << error << std::dec << ")\n";
    emitEvent("disconnected", {{"error", error}});
}

void CameraDeviceWrapper::OnPropertyChanged() {
    emitEvent("property_changed");
}

void CameraDeviceWrapper::OnPropertyChangedCodes(CrInt32u num, CrInt32u* codes) {
    nlohmann::json codesArray = nlohmann::json::array();
    for (CrInt32u i = 0; i < num; i++) {
        codesArray.push_back(codes[i]);
    }
    emitEvent("property_changed", {{"codes", codesArray}});
}

void CameraDeviceWrapper::OnLvPropertyChanged() {
    emitEvent("lv_property_changed");
}

void CameraDeviceWrapper::OnLvPropertyChangedCodes(CrInt32u num, CrInt32u* codes) {
    nlohmann::json codesArray = nlohmann::json::array();
    for (CrInt32u i = 0; i < num; i++) {
        codesArray.push_back(codes[i]);
    }
    emitEvent("lv_property_changed", {{"codes", codesArray}});
}

void CameraDeviceWrapper::OnCompleteDownload(CrChar* filename, CrInt32u type) {
    std::string filenameStr;
    if (filename) {
        const CrChar* p = filename;
        while (*p) {
            filenameStr += static_cast<char>(*p);
            p++;
        }
    }
    std::cout << "[Camera " << m_index << "] Download complete: " << filenameStr << "\n";
    emitEvent("capture_complete", {{"filename", filenameStr}, {"type", type}});
}

void CameraDeviceWrapper::OnNotifyContentsTransfer(CrInt32u notify, SDK::CrContentHandle handle, CrChar* filename) {
    std::string filenameStr;
    if (filename) {
        const CrChar* p = filename;
        while (*p) {
            filenameStr += static_cast<char>(*p);
            p++;
        }
    }
    emitEvent("content_transfer", {{"notify", notify}, {"handle", handle}, {"filename", filenameStr}});
}

void CameraDeviceWrapper::OnWarning(CrInt32u warning) {
    std::cout << "[Camera " << m_index << "] Warning: 0x" << std::hex << warning << std::dec << "\n";
    emitEvent("warning", {{"code", warning}});
}

void CameraDeviceWrapper::OnWarningExt(CrInt32u warning, CrInt32 param1, CrInt32 param2, CrInt32 param3) {
    emitEvent("warning_ext", {
        {"code", warning},
        {"param1", param1},
        {"param2", param2},
        {"param3", param3}
    });
}

void CameraDeviceWrapper::OnError(CrInt32u error) {
    std::cerr << "[Camera " << m_index << "] Error: 0x" << std::hex << error << std::dec << "\n";
    emitEvent("error", {{"code", error}});
}

void CameraDeviceWrapper::emitEvent(const std::string& type, const nlohmann::json& data) {
    if (m_eventCallback) {
        CameraEvent event(type, m_index);
        event.data = data;
        m_eventCallback(event);
    }
}

} // namespace crsdk_rest
