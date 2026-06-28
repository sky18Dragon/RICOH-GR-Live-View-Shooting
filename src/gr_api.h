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
  void parsePropsJson(const String& json, CameraProps& props) const;
  void setError(const String& message);

  WiFiClient _liveClient;
  bool _liveViewOpen = false;
  String _host;
  uint16_t _port = 80;
  String _lastError;
};
