#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

struct CameraProps {
  bool ok = false;
  String model;
  String battery;
};

class GrApi {
public:
  void setEndpoint(const char* host, uint16_t port = 80);
  bool fetchProps(CameraProps& props, uint32_t timeoutMs);
  // POST /v1/camera/shoot. The camera exposes the shutter over Wi-Fi as well
  // as BLE, which is what lets the BLE link be released while streaming.
  bool shoot(uint32_t timeoutMs);
  // GR II follows the official GR Remote sequence: try the one-shot endpoint
  // with camera autofocus, then fall back to start + finish when the camera
  // reports the legacy precondition response.
  bool shootGr2(uint32_t timeoutMs);
  // Ask the camera to shut down its WLAN AP. A dropped connection after the
  // request is expected because the endpoint tears down its own transport.
  bool finishWlan(uint32_t timeoutMs);
  bool openLiveView();
  void closeLiveView();
  bool isLiveViewOpen();
  int readLiveView(uint8_t* dst, size_t len);
  const String& lastError() const;

private:
  bool connectClient(WiFiClient& client, uint32_t timeoutMs);
  bool readHttpHeaders(WiFiClient& client, uint32_t timeoutMs, String& headers);
  int parseHttpStatus(const String& headers) const;
  int parseContentLength(const String& headers) const;
  bool postGr2Command(const char* path, uint32_t timeoutMs, String& responseBody);
  void parsePropsJson(const String& json, CameraProps& props) const;
  void setError(const String& message);

  WiFiClient _liveClient;
  bool _liveViewOpen = false;
  String _host;
  uint16_t _port = 80;
  String _lastError;
};
