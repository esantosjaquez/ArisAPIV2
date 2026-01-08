#include "api/JsonHelpers.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace crsdk_rest {

nlohmann::json jsonSuccess(const nlohmann::json& data) {
    nlohmann::json result;
    result["success"] = true;

    if (!data.is_null()) {
        result["data"] = data;
    }

    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%FT%TZ");
    result["timestamp"] = ss.str();

    return result;
}

nlohmann::json jsonError(int code, const std::string& message) {
    nlohmann::json result;
    result["success"] = false;
    result["error"]["code"] = code;
    result["error"]["message"] = message;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%FT%TZ");
    result["timestamp"] = ss.str();

    return result;
}

nlohmann::json jsonError(uint32_t sdkError, const std::string& context) {
    int httpStatus = mapSdkErrorToHttp(sdkError);
    std::string message = context.empty()
        ? getSdkErrorName(sdkError)
        : context + ": " + getSdkErrorName(sdkError);

    nlohmann::json result;
    result["success"] = false;
    result["error"]["code"] = sdkError;
    result["error"]["httpStatus"] = httpStatus;
    result["error"]["message"] = message;
    result["error"]["sdkError"] = getSdkErrorName(sdkError);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%FT%TZ");
    result["timestamp"] = ss.str();

    return result;
}

int mapSdkErrorToHttp(uint32_t sdkError) {
    // Error categories from CrError.h
    uint32_t category = sdkError & 0xFF00;

    switch (category) {
        case 0x0000:  // CrError_None
            return 200;

        case 0x8200:  // CrError_Connect
            switch (sdkError) {
                case 0x8201:  // TimeOut
                    return 504;
                case 0x8204:  // FailRejected
                case 0x8207:  // SessionAlreadyOpened
                    return 403;
                case 0x8205:  // FailBusy
                    return 409;
                default:
                    return 500;
            }

        case 0x8400:  // CrError_Api
            return 400;

        case 0x8300:  // CrError_Memory
            return 503;

        case 0x8700:  // CrError_Adaptor
            switch (sdkError) {
                case 0x8702:  // DeviceBusy
                    return 409;
                default:
                    return 500;
            }

        case 0x8800:  // CrError_Device
            return 500;

        default:
            return 500;
    }
}

std::string getSdkErrorName(uint32_t sdkError) {
    switch (sdkError) {
        case 0x0000: return "CrError_None";

        // Generic errors
        case 0x8000: return "CrError_Generic";
        case 0x8001: return "CrError_Generic_InvalidHandle";
        case 0x8002: return "CrError_Generic_InvalidParameter";
        case 0x8003: return "CrError_Generic_NotSupported";
        case 0x8004: return "CrError_Generic_MemoryError";
        case 0x8005: return "CrError_Generic_Unknown";
        case 0x8006: return "CrError_Generic_Abort";

        // File errors
        case 0x8100: return "CrError_File";
        case 0x8101: return "CrError_File_EOF";
        case 0x8102: return "CrError_File_OutOfRange";
        case 0x8103: return "CrError_File_NotFound";
        case 0x8104: return "CrError_File_StorageFull";
        case 0x8105: return "CrError_File_PermissionDenied";

        // Connect errors
        case 0x8200: return "CrError_Connect";
        case 0x8201: return "CrError_Connect_TimeOut";
        case 0x8202: return "CrError_Connect_Disconnected";
        case 0x8204: return "CrError_Connect_FailRejected";
        case 0x8205: return "CrError_Connect_FailBusy";
        case 0x8206: return "CrError_Connect_NoDevice";
        case 0x8207: return "CrError_Connect_SessionAlreadyOpened";
        case 0x8208: return "CrError_Connect_InvalidHandle";
        case 0x8209: return "CrError_Connect_Reconnecting";

        // Memory errors
        case 0x8300: return "CrError_Memory";
        case 0x8301: return "CrError_Memory_OutOfMemory";
        case 0x8302: return "CrError_Memory_Insufficient";

        // API errors
        case 0x8400: return "CrError_Api";
        case 0x8401: return "CrError_Api_Insufficient";
        case 0x8402: return "CrError_Api_InvalidCalled";

        // Adaptor errors
        case 0x8700: return "CrError_Adaptor";
        case 0x8701: return "CrError_Adaptor_InvalidProperty";
        case 0x8702: return "CrError_Adaptor_DeviceBusy";

        // Device errors
        case 0x8800: return "CrError_Device";
        case 0x8801: return "CrError_Device_CameraStatusError";

        default: {
            std::stringstream ss;
            ss << "CrError_0x" << std::hex << std::uppercase << sdkError;
            return ss.str();
        }
    }
}

std::string getPropertyName(uint32_t code) {
    // Common property codes from CrDeviceProperty.h
    switch (code) {
        case 0x0100: return "FNumber";
        case 0x0101: return "ExposureBiasCompensation";
        case 0x0102: return "FlashCompensation";
        case 0x0103: return "ShutterSpeed";
        case 0x0104: return "IsoSensitivity";
        case 0x0105: return "ExposureProgramMode";
        case 0x0106: return "FileType";
        case 0x0107: return "JpegQuality";
        case 0x0108: return "WhiteBalance";
        case 0x0109: return "FocusMode";
        case 0x010A: return "MeteringMode";
        case 0x010B: return "FlashMode";
        case 0x010D: return "DriveMode";
        case 0x0110: return "FocusArea";
        case 0x0115: return "Colortemp";
        case 0x0119: return "StillImageQuality";
        case 0x012B: return "NearFar";
        case 0x0131: return "DateTime_Settings";
        case 0x0138: return "AFTrackingSensitivity";
        case 0x013C: return "AF_Area_Position";
        case 0x0144: return "Zoom_Scale";
        case 0x0145: return "Zoom_Setting";
        case 0x0146: return "Zoom_Operation";
        case 0x0201: return "MediaSLOT1_Status";
        case 0x0202: return "MediaSLOT2_Status";
        case 0x0206: return "MediaSLOT1_RemainingTime";
        case 0x0207: return "MediaSLOT2_RemainingTime";
        case 0x0301: return "Movie_File_Format";
        case 0x0302: return "Movie_Recording_Setting";
        case 0x0500: return "BatteryRemain";
        case 0x0501: return "BatteryLevel";
        case 0x0510: return "LiveView_Status";
        case 0x0520: return "FocusIndication";
        case 0x0532: return "RecordingState";
        default: {
            std::stringstream ss;
            ss << "Property_0x" << std::hex << std::uppercase << code;
            return ss.str();
        }
    }
}

std::string formatPropertyValue(uint32_t code, uint64_t value) {
    switch (code) {
        case 0x0100: {  // FNumber
            // Value is f-number * 100 (e.g., 280 = F2.8)
            double fnum = static_cast<double>(value) / 100.0;
            std::stringstream ss;
            ss << "F" << std::fixed << std::setprecision(1) << fnum;
            return ss.str();
        }
        case 0x0103: {  // ShutterSpeed
            // Value encodes numerator/denominator
            uint32_t num = (value >> 16) & 0xFFFF;
            uint32_t den = value & 0xFFFF;
            if (den == 1) {
                return std::to_string(num) + "s";
            } else {
                return "1/" + std::to_string(den) + "s";
            }
        }
        case 0x0104: {  // ISO
            return "ISO " + std::to_string(value);
        }
        default:
            return std::to_string(value);
    }
}

} // namespace crsdk_rest
