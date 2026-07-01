#pragma once

#include <Arduino.h>
#include <WiFi.h>

class GrWifi {
public:
  using ConnectGuard = bool (*)();

  void begin();
  bool connect(const char* ssid, const char* password, uint32_t timeoutMs);
  bool connect(const char* ssid, const char* password, const char* bssid, uint32_t timeoutMs);
  bool connect(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs);
  bool connect(const char* ssid, const char* password, uint32_t timeoutMs, ConnectGuard guard);
  bool connect(const char* ssid, const char* password, const char* bssid, uint32_t timeoutMs, ConnectGuard guard);
  bool connect(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs, ConnectGuard guard);
  bool isConnected() const;
  void disconnect();

  String ssid() const;
  String bssidString() const;
  int32_t rssi() const;
  String localIPString() const;
  String statusText() const;

private:
  bool connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs);
  bool connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs, ConnectGuard guard);

  wl_status_t _lastStatus = WL_IDLE_STATUS;
};
