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
  uint8_t channel() const;
  int32_t rssi() const;
  String localIPString() const;
  String statusText() const;

  // STA disconnect events since boot, including ones this firmware asked for
  // (portrait handoff, reconnects), and the reason code of the latest one.
  uint32_t disconnectCount() const;
  int lastDisconnectReason() const;
  static const char* disconnectReasonName(int reason);
  // Configured Wi-Fi TX power ceiling in dBm, or a negative value if unknown.
  float txPowerDbm() const;

private:
  // Coexistence-oriented link settings: minimum modem sleep (required with
  // BLE on), 20 MHz STA bandwidth and the maximum TX power. Idempotent; must
  // run while the STA interface is started.
  void applyLinkTuning(const char* reason);

  bool connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs);
  bool connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs, ConnectGuard guard);

  wl_status_t _lastStatus = WL_IDLE_STATUS;
};
