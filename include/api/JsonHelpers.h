#pragma once

#include <string>
#include <json.hpp>

namespace crsdk_rest {

// Create success response
nlohmann::json jsonSuccess(const nlohmann::json& data = nullptr);

// Create error response
nlohmann::json jsonError(int code, const std::string& message);
nlohmann::json jsonError(uint32_t sdkError, const std::string& context = "");

// Map SDK error to HTTP status
int mapSdkErrorToHttp(uint32_t sdkError);

// Get SDK error name
std::string getSdkErrorName(uint32_t sdkError);

// Property code to name mapping
std::string getPropertyName(uint32_t code);

// Format property value for display
std::string formatPropertyValue(uint32_t code, uint64_t value);

} // namespace crsdk_rest
