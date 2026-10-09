#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>

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
  uint8_t channel() const;
  int32_t rssi() const;
  String localIPString() const;
  String statusText() const;
  int lastDisconnectReason() const;
  int lastDisconnectRssi() const;
  String lastDisconnectText() const;
  uint32_t disconnectSequence() const;

private:
  bool connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs);
  bool connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs, ConnectGuard guard);
  void handleWifiEvent(arduino_event_id_t event, arduino_event_info_t info);

  wl_status_t _lastStatus = WL_IDLE_STATUS;
  bool _eventsRegistered = false;
  std::atomic<uint32_t> _disconnectSequence{0};
  std::atomic<int> _lastDisconnectReason{0};
  std::atomic<int> _lastDisconnectRssi{0};
  std::atomic<uint32_t> _staConnectedAtMs{0};
  std::atomic<uint32_t> _gotIpAtMs{0};
  std::atomic<uint8_t> _eventChannel{0};
};
