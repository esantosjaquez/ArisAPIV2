#include "api/ApiRouter.h"
#include "api/JsonHelpers.h"
#include "camera/CameraManager.h"
#include "grbl/GrblController.h"
#include "server/MjpegStreamer.h"
#include <iostream>

namespace crsdk_rest {

static MjpegStreamer* s_mjpegStreamer = nullptr;

void ApiRouter::setupRoutes(httplib::Server& server, MjpegStreamer* streamer) {
    s_mjpegStreamer = streamer;

    // Health check
    server.Get("/health", handleHealth);
    server.Get("/api/v1/health", handleHealth);

    // SDK endpoints
    server.Post("/api/v1/sdk/init", handleSdkInit);
    server.Post("/api/v1/sdk/release", handleSdkRelease);
    server.Get("/api/v1/sdk/version", handleSdkVersion);

    // Camera endpoints
    server.Get("/api/v1/cameras", handleListCameras);
    server.Get("/api/v1/cameras/connected", handleConnectedCameras);
    server.Post(R"(/api/v1/cameras/(\d+)/connect)", handleConnectCamera);
    server.Post(R"(/api/v1/cameras/(\d+)/disconnect)", handleDisconnectCamera);

    // Property endpoints
    server.Get(R"(/api/v1/cameras/(\d+)/properties)", handleGetProperties);
    server.Put(R"(/api/v1/cameras/(\d+)/properties/(\d+))", handleSetProperty);

    // Command endpoints
    server.Post(R"(/api/v1/cameras/(\d+)/command)", handleSendCommand);
    server.Post(R"(/api/v1/cameras/(\d+)/capture)", handleCapture);
    server.Post(R"(/api/v1/cameras/(\d+)/record/start)", handleRecordStart);
    server.Post(R"(/api/v1/cameras/(\d+)/record/stop)", handleRecordStop);
    server.Post(R"(/api/v1/cameras/(\d+)/focus)", handleFocus);

    // Live view endpoints
    server.Get(R"(/api/v1/cameras/(\d+)/liveview/image)", handleLiveViewImage);
    server.Get(R"(/api/v1/cameras/(\d+)/liveview/info)", handleLiveViewInfo);
    server.Get(R"(/api/v1/cameras/(\d+)/liveview/stream)", [](const httplib::Request& req, httplib::Response& res) {
        if (s_mjpegStreamer) {
            s_mjpegStreamer->handleStream(req, res);
        } else {
            res.status = 500;
            res.set_content("{\"error\": \"MJPEG streamer not available\"}", "application/json");
        }
    });

    // Content transfer endpoints
    server.Get(R"(/api/v1/cameras/(\d+)/contents/folders)", handleGetFolders);
    server.Get(R"(/api/v1/cameras/(\d+)/contents/folders/(\d+))", handleGetContents);
    server.Get(R"(/api/v1/cameras/(\d+)/contents/(\d+)/info)", handleGetContentInfo);
    server.Get(R"(/api/v1/cameras/(\d+)/contents/(\d+)/download)", handleDownloadContent);
    server.Get(R"(/api/v1/cameras/(\d+)/contents/(\d+)/thumbnail)", handleGetThumbnail);

    // GRBL/CNC endpoints
    server.Get("/api/v1/grbl/ports", handleGrblListPorts);
    server.Post("/api/v1/grbl/connect", handleGrblConnect);
    server.Post("/api/v1/grbl/disconnect", handleGrblDisconnect);
    server.Get("/api/v1/grbl/status", handleGrblStatus);
    server.Post("/api/v1/grbl/home", handleGrblHome);
    server.Post("/api/v1/grbl/move", handleGrblMove);
    server.Post("/api/v1/grbl/jog", handleGrblJog);
    server.Post("/api/v1/grbl/stop", handleGrblStop);
    server.Post("/api/v1/grbl/resume", handleGrblResume);
    server.Post("/api/v1/grbl/reset", handleGrblReset);
    server.Post("/api/v1/grbl/unlock", handleGrblUnlock);
    server.Get("/api/v1/grbl/settings", handleGrblSettings);
    server.Put(R"(/api/v1/grbl/settings/(\d+))", handleGrblSetSetting);
    server.Post("/api/v1/grbl/command", handleGrblCommand);

    std::cout << "[ApiRouter] Routes configured\n";
}

// Health check
void ApiRouter::handleHealth(const httplib::Request&, httplib::Response& res) {
    auto& manager = CameraManager::getInstance();
    nlohmann::json data;
    data["status"] = "ok";
    data["sdkInitialized"] = manager.isInitialized();
    data["connectedCameras"] = manager.getConnectedCameraIndices().size();
    res.set_content(jsonSuccess(data).dump(), "application/json");
}

// SDK endpoints
void ApiRouter::handleSdkInit(const httplib::Request& req, httplib::Response& res) {
    uint32_t logType = 0;
    if (!req.body.empty()) {
        try {
            auto json = nlohmann::json::parse(req.body);
            if (json.contains("logType")) {
                logType = json["logType"].get<uint32_t>();
            }
        } catch (...) {}
    }

    auto& manager = CameraManager::getInstance();
    if (manager.initialize(logType)) {
        res.set_content(jsonSuccess({{"initialized", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to initialize SDK").dump(), "application/json");
    }
}

void ApiRouter::handleSdkRelease(const httplib::Request&, httplib::Response& res) {
    auto& manager = CameraManager::getInstance();
    manager.shutdown();
    res.set_content(jsonSuccess({{"released", true}}).dump(), "application/json");
}

void ApiRouter::handleSdkVersion(const httplib::Request&, httplib::Response& res) {
    auto& manager = CameraManager::getInstance();
    uint32_t version = manager.getSDKVersion();

    nlohmann::json data;
    data["version"] = version;
    data["major"] = (version >> 24) & 0xFF;
    data["minor"] = (version >> 16) & 0xFF;
    data["patch"] = version & 0xFFFF;

    res.set_content(jsonSuccess(data).dump(), "application/json");
}

// Camera endpoints
void ApiRouter::handleListCameras(const httplib::Request& req, httplib::Response& res) {
    auto& manager = CameraManager::getInstance();

    if (!manager.isInitialized()) {
        res.status = 400;
        res.set_content(jsonError(400, "SDK not initialized").dump(), "application/json");
        return;
    }

    uint8_t timeout = 3;
    if (req.has_param("timeout")) {
        try {
            timeout = static_cast<uint8_t>(std::stoi(req.get_param_value("timeout")));
        } catch (...) {}
    }

    auto cameras = manager.enumerateCameras(timeout);

    nlohmann::json camerasJson = nlohmann::json::array();
    for (const auto& cam : cameras) {
        nlohmann::json camJson;
        camJson["index"] = cam.index;
        camJson["id"] = cam.id;
        camJson["model"] = cam.model;
        camJson["connectionType"] = cam.connectionType;
        camJson["sshSupported"] = cam.sshSupported;
        camerasJson.push_back(camJson);
    }

    res.set_content(jsonSuccess({{"cameras", camerasJson}}).dump(), "application/json");
}

void ApiRouter::handleConnectedCameras(const httplib::Request&, httplib::Response& res) {
    auto& manager = CameraManager::getInstance();
    auto indices = manager.getConnectedCameraIndices();

    nlohmann::json camerasJson = nlohmann::json::array();
    for (int idx : indices) {
        auto camera = manager.getConnectedCamera(idx);
        if (camera) {
            nlohmann::json camJson;
            camJson["index"] = idx;
            camJson["model"] = camera->getModel();
            camJson["connected"] = camera->isConnected();
            camerasJson.push_back(camJson);
        }
    }

    res.set_content(jsonSuccess({{"cameras", camerasJson}}).dump(), "application/json");
}

void ApiRouter::handleConnectCamera(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    int mode = 0;  // Default: Remote
    bool reconnect = true;

    if (!req.body.empty()) {
        try {
            auto json = nlohmann::json::parse(req.body);
            if (json.contains("mode")) {
                std::string modeStr = json["mode"].get<std::string>();
                if (modeStr == "contents_transfer" || modeStr == "ContentsTransfer") {
                    mode = 1;
                }
            }
            if (json.contains("reconnect")) {
                reconnect = json["reconnect"].get<bool>();
            }
        } catch (...) {}
    }

    auto& manager = CameraManager::getInstance();
    auto camera = manager.connectCamera(cameraIndex, mode, reconnect);

    if (camera && camera->isConnected()) {
        nlohmann::json data;
        data["connected"] = true;
        data["index"] = cameraIndex;
        data["model"] = camera->getModel();
        res.set_content(jsonSuccess(data).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to connect to camera").dump(), "application/json");
    }
}

void ApiRouter::handleDisconnectCamera(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    manager.disconnectCamera(cameraIndex);

    res.set_content(jsonSuccess({{"disconnected", true}}).dump(), "application/json");
}

// Property endpoints
void ApiRouter::handleGetProperties(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    nlohmann::json properties;

    // Check if specific codes requested
    if (req.has_param("codes")) {
        std::vector<uint32_t> codes;
        std::string codesStr = req.get_param_value("codes");
        std::stringstream ss(codesStr);
        std::string token;
        while (std::getline(ss, token, ',')) {
            try {
                codes.push_back(static_cast<uint32_t>(std::stoul(token)));
            } catch (...) {}
        }
        properties = camera->getSelectProperties(codes);
    } else {
        properties = camera->getAllProperties();
    }

    res.set_content(jsonSuccess({{"properties", properties}}).dump(), "application/json");
}

void ApiRouter::handleSetProperty(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);
    uint32_t propertyCode = static_cast<uint32_t>(std::stoul(req.matches[2]));

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    uint64_t value = 0;
    try {
        auto json = nlohmann::json::parse(req.body);
        value = json["value"].get<uint64_t>();
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(jsonError(400, std::string("Invalid request: ") + e.what()).dump(), "application/json");
        return;
    }

    if (camera->setProperty(propertyCode, value)) {
        res.set_content(jsonSuccess({{"set", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to set property").dump(), "application/json");
    }
}

// Command endpoints
void ApiRouter::handleSendCommand(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    uint32_t commandId = 0;
    uint32_t param = 0;

    try {
        auto json = nlohmann::json::parse(req.body);
        commandId = json["commandId"].get<uint32_t>();
        if (json.contains("param")) {
            param = json["param"].get<uint32_t>();
        }
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(jsonError(400, std::string("Invalid request: ") + e.what()).dump(), "application/json");
        return;
    }

    if (camera->sendCommand(commandId, param)) {
        res.set_content(jsonSuccess({{"sent", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to send command").dump(), "application/json");
    }
}

void ApiRouter::handleCapture(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    if (camera->capture()) {
        res.set_content(jsonSuccess({{"captured", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to capture").dump(), "application/json");
    }
}

void ApiRouter::handleRecordStart(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    if (camera->startRecording()) {
        res.set_content(jsonSuccess({{"recording", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to start recording").dump(), "application/json");
    }
}

void ApiRouter::handleRecordStop(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    if (camera->stopRecording()) {
        res.set_content(jsonSuccess({{"recording", false}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to stop recording").dump(), "application/json");
    }
}

void ApiRouter::handleFocus(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    std::string action = "half_press";
    if (!req.body.empty()) {
        try {
            auto json = nlohmann::json::parse(req.body);
            if (json.contains("action")) {
                action = json["action"].get<std::string>();
            }
        } catch (...) {}
    }

    bool success = false;
    if (action == "half_press") {
        success = camera->halfPressShutter();
    } else if (action == "release") {
        success = camera->releaseShutter();
    }

    if (success) {
        res.set_content(jsonSuccess({{"focus", action}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Focus command failed").dump(), "application/json");
    }
}

// Live view endpoints
void ApiRouter::handleLiveViewImage(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    auto imageData = camera->getLiveViewImage();
    if (imageData.empty()) {
        res.status = 204;  // No Content
        return;
    }

    res.set_content(reinterpret_cast<const char*>(imageData.data()), imageData.size(), "image/jpeg");
}

void ApiRouter::handleLiveViewInfo(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    auto info = camera->getLiveViewInfo();
    res.set_content(jsonSuccess(info).dump(), "application/json");
}

// Content transfer endpoints
void ApiRouter::handleGetFolders(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    auto folders = camera->getDateFolderList();
    res.set_content(jsonSuccess({{"folders", folders}}).dump(), "application/json");
}

void ApiRouter::handleGetContents(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);
    uint32_t folderHandle = static_cast<uint32_t>(std::stoul(req.matches[2]));

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    auto contents = camera->getContentsHandleList(folderHandle);
    res.set_content(jsonSuccess({{"contents", contents}}).dump(), "application/json");
}

void ApiRouter::handleGetContentInfo(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);
    uint32_t contentHandle = static_cast<uint32_t>(std::stoul(req.matches[2]));

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    auto info = camera->getContentsDetailInfo(contentHandle);
    res.set_content(jsonSuccess(info).dump(), "application/json");
}

void ApiRouter::handleDownloadContent(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);
    uint32_t contentHandle = static_cast<uint32_t>(std::stoul(req.matches[2]));

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    // For now, return thumbnail as a simple implementation
    auto imageData = camera->getThumbnail(contentHandle);
    if (imageData.empty()) {
        res.status = 404;
        res.set_content(jsonError(404, "Content not found").dump(), "application/json");
        return;
    }

    res.set_content(reinterpret_cast<const char*>(imageData.data()), imageData.size(), "image/jpeg");
}

void ApiRouter::handleGetThumbnail(const httplib::Request& req, httplib::Response& res) {
    int cameraIndex = std::stoi(req.matches[1]);
    uint32_t contentHandle = static_cast<uint32_t>(std::stoul(req.matches[2]));

    auto& manager = CameraManager::getInstance();
    auto camera = manager.getConnectedCamera(cameraIndex);

    if (!camera) {
        res.status = 404;
        res.set_content(jsonError(404, "Camera not connected").dump(), "application/json");
        return;
    }

    auto imageData = camera->getThumbnail(contentHandle);
    if (imageData.empty()) {
        res.status = 404;
        res.set_content(jsonError(404, "Thumbnail not found").dump(), "application/json");
        return;
    }

    res.set_content(reinterpret_cast<const char*>(imageData.data()), imageData.size(), "image/jpeg");
}

// GRBL/CNC endpoints
void ApiRouter::handleGrblListPorts(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();
    auto ports = grbl.listPorts();

    nlohmann::json portsJson = nlohmann::json::array();
    for (const auto& port : ports) {
        portsJson.push_back(port);
    }

    res.set_content(jsonSuccess({{"ports", portsJson}}).dump(), "application/json");
}

void ApiRouter::handleGrblConnect(const httplib::Request& req, httplib::Response& res) {
    std::string port;
    int baudRate = 115200;

    if (!req.body.empty()) {
        try {
            auto json = nlohmann::json::parse(req.body);
            if (json.contains("port")) {
                port = json["port"].get<std::string>();
            }
            if (json.contains("baudRate")) {
                baudRate = json["baudRate"].get<int>();
            }
        } catch (...) {}
    }

    auto& grbl = GrblController::getInstance();
    if (grbl.connect(port, baudRate)) {
        nlohmann::json data;
        data["connected"] = true;
        data["port"] = grbl.getPort();
        data["version"] = grbl.getVersion();
        res.set_content(jsonSuccess(data).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to connect to GRBL device").dump(), "application/json");
    }
}

void ApiRouter::handleGrblDisconnect(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();
    grbl.disconnect();
    res.set_content(jsonSuccess({{"disconnected", true}}).dump(), "application/json");
}

void ApiRouter::handleGrblStatus(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    auto status = grbl.getStatusJson();
    res.set_content(jsonSuccess(status).dump(), "application/json");
}

void ApiRouter::handleGrblHome(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    if (grbl.home()) {
        res.set_content(jsonSuccess({{"command", "$H"}, {"response", "ok"}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Homing failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblMove(const httplib::Request& req, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    std::string type = "G0";
    std::optional<double> x, y, z;
    double feed = 1000;

    try {
        auto json = nlohmann::json::parse(req.body);
        if (json.contains("type")) {
            type = json["type"].get<std::string>();
        }
        if (json.contains("x") && !json["x"].is_null()) {
            x = json["x"].get<double>();
        }
        if (json.contains("y") && !json["y"].is_null()) {
            y = json["y"].get<double>();
        }
        if (json.contains("z") && !json["z"].is_null()) {
            z = json["z"].get<double>();
        }
        if (json.contains("feed")) {
            feed = json["feed"].get<double>();
        }
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(jsonError(400, std::string("Invalid request: ") + e.what()).dump(), "application/json");
        return;
    }

    bool success = false;
    std::string command;

    if (type == "G0" || type == "g0") {
        success = grbl.moveG0(x, y, z);
        command = "G0";
    } else if (type == "G1" || type == "g1") {
        success = grbl.moveG1(x, y, z, feed);
        command = "G1";
    } else {
        res.status = 400;
        res.set_content(jsonError(400, "Invalid move type. Use G0 or G1").dump(), "application/json");
        return;
    }

    if (success) {
        nlohmann::json data;
        data["command"] = command;
        if (x) data["x"] = *x;
        if (y) data["y"] = *y;
        if (z) data["z"] = *z;
        if (type == "G1" || type == "g1") data["feed"] = feed;
        data["response"] = "ok";
        res.set_content(jsonSuccess(data).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Move command failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblJog(const httplib::Request& req, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    char axis = 'X';
    double distance = 0;
    double feed = 1000;

    try {
        auto json = nlohmann::json::parse(req.body);
        if (json.contains("axis")) {
            std::string axisStr = json["axis"].get<std::string>();
            if (!axisStr.empty()) {
                axis = std::toupper(axisStr[0]);
            }
        }
        if (json.contains("distance")) {
            distance = json["distance"].get<double>();
        }
        if (json.contains("feed")) {
            feed = json["feed"].get<double>();
        }
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(jsonError(400, std::string("Invalid request: ") + e.what()).dump(), "application/json");
        return;
    }

    if (grbl.jog(axis, distance, feed)) {
        nlohmann::json data;
        data["axis"] = std::string(1, axis);
        data["distance"] = distance;
        data["feed"] = feed;
        data["response"] = "ok";
        res.set_content(jsonSuccess(data).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Jog command failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblStop(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    if (grbl.feedHold()) {
        res.set_content(jsonSuccess({{"command", "!"}, {"state", "Hold"}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Feed hold failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblResume(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    if (grbl.cycleStart()) {
        res.set_content(jsonSuccess({{"command", "~"}, {"state", "Run"}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Cycle start failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblReset(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    if (grbl.softReset()) {
        res.set_content(jsonSuccess({{"command", "0x18"}, {"reset", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Soft reset failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblUnlock(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    if (grbl.unlock()) {
        res.set_content(jsonSuccess({{"command", "$X"}, {"unlocked", true}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Unlock failed").dump(), "application/json");
    }
}

void ApiRouter::handleGrblSettings(const httplib::Request&, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    auto settings = grbl.getSettingsJson();
    res.set_content(jsonSuccess({{"settings", settings}}).dump(), "application/json");
}

void ApiRouter::handleGrblSetSetting(const httplib::Request& req, httplib::Response& res) {
    int settingId = std::stoi(req.matches[1]);

    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    double value = 0;
    try {
        auto json = nlohmann::json::parse(req.body);
        value = json["value"].get<double>();
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(jsonError(400, std::string("Invalid request: ") + e.what()).dump(), "application/json");
        return;
    }

    if (grbl.setSetting(settingId, value)) {
        nlohmann::json data;
        data["command"] = "$" + std::to_string(settingId) + "=" + std::to_string(value);
        data["response"] = "ok";
        res.set_content(jsonSuccess(data).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(jsonError(500, "Failed to set setting").dump(), "application/json");
    }
}

void ApiRouter::handleGrblCommand(const httplib::Request& req, httplib::Response& res) {
    auto& grbl = GrblController::getInstance();

    if (!grbl.isConnected()) {
        res.status = 400;
        res.set_content(jsonError(400, "GRBL not connected").dump(), "application/json");
        return;
    }

    std::string command;
    int timeout = 5000;

    try {
        auto json = nlohmann::json::parse(req.body);
        command = json["command"].get<std::string>();
        if (json.contains("timeout")) {
            timeout = json["timeout"].get<int>();
        }
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(jsonError(400, std::string("Invalid request: ") + e.what()).dump(), "application/json");
        return;
    }

    std::string response = grbl.sendCommand(command, timeout);

    nlohmann::json data;
    data["command"] = command;
    data["response"] = response;
    res.set_content(jsonSuccess(data).dump(), "application/json");
}

} // namespace crsdk_rest
