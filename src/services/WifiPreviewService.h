#pragma once

#include <Arduino.h>

#include "../core/Result.h"
#include "../gr_api.h"
#include "../gr_wifi.h"
#include "../mjpeg_stream.h"

namespace rvf {

class WifiPreviewService {
public:
    WifiPreviewService() = default;
    WifiPreviewService(GrWifi& wifi, GrApi& api, MjpegStream& stream) {
        attach(wifi, api, stream);
    }

    void attach(GrWifi& wifi, GrApi& api, MjpegStream& stream);
    bool attached() const;

    void setEndpoint(const char* host, uint16_t port);

    Result activateWifi();
    Result connectWifi(const char* ssid,
                       const char* password,
                       const char* bssid,
                       uint8_t channel,
                       uint32_t timeoutMs,
                       GrWifi::ConnectGuard guard = nullptr);
    void disconnectWifi();
    bool isWifiConnected() const;

    Result fetchProps(CameraProps& props, uint32_t timeoutMs);
    Result startPreview();
    void stopPreview();
    bool isPreviewRunning() const;
    int readFrame(uint8_t* dst, size_t len);
    size_t processFrameData(const uint8_t* data, size_t len);

    void resetStats(uint32_t nowMs = 0);
    void recordRenderedFrame(uint32_t decodeMs,
                             uint32_t renderMs,
                             int jpegWidth,
                             int jpegHeight);
    void logStatsIfDue(uint32_t nowMs, uint32_t intervalMs = 5000);

    uint32_t lastReadMs() const { return _lastReadMs; }
    uint32_t lastProcessMs() const { return _lastProcessMs; }
    uint32_t lastDecodeMs() const { return _lastDecodeMs; }
    uint32_t lastRenderMs() const { return _lastRenderMs; }
    const String& lastError() const { return _lastError; }

private:
    Result requireAttached(const char* operation) const;
    void setError(const String& message);

    GrWifi* _wifi = nullptr;
    GrApi* _api = nullptr;
    MjpegStream* _stream = nullptr;
    String _lastError = "WiFi preview service not attached";

    uint32_t _statsWindowStartMs = 0;
    uint32_t _lastStatsLogMs = 0;
    uint32_t _lastReadMs = 0;
    uint32_t _lastProcessMs = 0;
    uint32_t _lastDecodeMs = 0;
    uint32_t _lastRenderMs = 0;
    uint32_t _framesInWindow = 0;
    uint32_t _bytesInWindow = 0;
    uint32_t _decodeMsTotal = 0;
    uint32_t _renderMsTotal = 0;
    uint32_t _maxDecodeMs = 0;
    uint32_t _maxRenderMs = 0;
    int _jpegWidth = 0;
    int _jpegHeight = 0;
};

}  // namespace rvf
