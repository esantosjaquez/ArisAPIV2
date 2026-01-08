#pragma once

// Ensure SSL support is disabled in httplib
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

namespace crsdk_rest {

class MjpegStreamer;

class ApiRouter {
public:
    static void setupRoutes(httplib::Server& server, MjpegStreamer* streamer);

private:
    // SDK endpoints
    static void handleSdkInit(const httplib::Request& req, httplib::Response& res);
    static void handleSdkRelease(const httplib::Request& req, httplib::Response& res);
    static void handleSdkVersion(const httplib::Request& req, httplib::Response& res);

    // Camera endpoints
    static void handleListCameras(const httplib::Request& req, httplib::Response& res);
    static void handleConnectedCameras(const httplib::Request& req, httplib::Response& res);
    static void handleConnectCamera(const httplib::Request& req, httplib::Response& res);
    static void handleDisconnectCamera(const httplib::Request& req, httplib::Response& res);

    // Property endpoints
    static void handleGetProperties(const httplib::Request& req, httplib::Response& res);
    static void handleSetProperty(const httplib::Request& req, httplib::Response& res);

    // Command endpoints
    static void handleSendCommand(const httplib::Request& req, httplib::Response& res);
    static void handleCapture(const httplib::Request& req, httplib::Response& res);
    static void handleRecordStart(const httplib::Request& req, httplib::Response& res);
    static void handleRecordStop(const httplib::Request& req, httplib::Response& res);
    static void handleFocus(const httplib::Request& req, httplib::Response& res);

    // Live view endpoints
    static void handleLiveViewImage(const httplib::Request& req, httplib::Response& res);
    static void handleLiveViewInfo(const httplib::Request& req, httplib::Response& res);

    // Content transfer endpoints
    static void handleGetFolders(const httplib::Request& req, httplib::Response& res);
    static void handleGetContents(const httplib::Request& req, httplib::Response& res);
    static void handleGetContentInfo(const httplib::Request& req, httplib::Response& res);
    static void handleDownloadContent(const httplib::Request& req, httplib::Response& res);
    static void handleGetThumbnail(const httplib::Request& req, httplib::Response& res);

    // Health check
    static void handleHealth(const httplib::Request& req, httplib::Response& res);

    // GRBL/CNC endpoints
    static void handleGrblListPorts(const httplib::Request& req, httplib::Response& res);
    static void handleGrblConnect(const httplib::Request& req, httplib::Response& res);
    static void handleGrblDisconnect(const httplib::Request& req, httplib::Response& res);
    static void handleGrblStatus(const httplib::Request& req, httplib::Response& res);
    static void handleGrblHome(const httplib::Request& req, httplib::Response& res);
    static void handleGrblMove(const httplib::Request& req, httplib::Response& res);
    static void handleGrblJog(const httplib::Request& req, httplib::Response& res);
    static void handleGrblStop(const httplib::Request& req, httplib::Response& res);
    static void handleGrblResume(const httplib::Request& req, httplib::Response& res);
    static void handleGrblReset(const httplib::Request& req, httplib::Response& res);
    static void handleGrblUnlock(const httplib::Request& req, httplib::Response& res);
    static void handleGrblSettings(const httplib::Request& req, httplib::Response& res);
    static void handleGrblSetSetting(const httplib::Request& req, httplib::Response& res);
    static void handleGrblCommand(const httplib::Request& req, httplib::Response& res);
};

} // namespace crsdk_rest
