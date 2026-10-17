#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

#include "liveview_focus_policy.h"
#include "liveview_reso_policy.h"

struct CameraProps {
  bool ok = false;
  String model;
  String battery;
  String focusSetting;
  LiveViewResolution resoList[LIVEVIEW_RESO_LIST_CAPACITY];
  size_t resoCount = 0;
};

class GrApi {
public:
  void setEndpoint(const char* host, uint16_t port = 80);
  bool fetchProps(CameraProps& props, uint32_t timeoutMs);
  // POST /v1/camera/shoot with form body af=on&pos=50,50. GR IIIx v1.60
  // parses those keys in the body; an empty body defaults to af=off.
  // GR III rejects the same fields as query parameters.
  bool shoot(uint32_t timeoutMs);
  // PUT /v1/params/lens with body focusSetting=<enum>. Must run before
  // GET /v1/liveview so it does not share the MJPEG keep-alive.
  bool setFocusSetting(const char* setting, uint32_t timeoutMs);
  // POST <path> with an empty body; the JSON reply lands in responseBody.
  bool postCommand(const char* path, uint32_t timeoutMs, String& responseBody);
  // Ask the camera to shut down its WLAN AP. A dropped connection after the
  // request is expected because the endpoint tears down its own transport.
  bool finishWlan(uint32_t timeoutMs);
  // 0 sends the bare /v1/liveview request; otherwise ?maxfps=<fps>, which the
  // camera applies as a send-side frame interval in SendFrameLoop.
  void setLiveViewMaxFps(uint8_t fps);
  // An invalid resolution omits reso=. A camera that rejects the requested
  // size gets one retry without it, and the size is dropped from then on.
  void setLiveViewResolution(const LiveViewResolution& resolution);
  bool openLiveView();
  void closeLiveView();
  bool isLiveViewOpen();
  int readLiveView(uint8_t* dst, size_t len);
  const String& lastError() const;
  void setError(const String& message);

private:
  bool connectClient(WiFiClient& client, uint32_t timeoutMs);
  bool readHttpHeaders(WiFiClient& client, uint32_t timeoutMs, String& headers);
  int parseHttpStatus(const String& headers) const;
  int parseContentLength(const String& headers) const;
  bool sendForm(const char* method, const char* path, const char* body, uint32_t timeoutMs);
  bool postForm(const char* path, const char* body, uint32_t timeoutMs);
  void parsePropsJson(const String& json, CameraProps& props) const;
  int requestLiveView(bool includeResolution);

  WiFiClient _liveClient;
  bool _liveViewOpen = false;
  uint8_t _liveViewMaxFps = 0;
  LiveViewResolution _liveViewResolution;
  String _host;
  uint16_t _port = 80;
  String _lastError;
};
