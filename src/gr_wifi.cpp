#include "gr_wifi.h"

#include <cstring>

#include "wifi_connection_policy.h"

namespace {
String wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return String("UNKNOWN(") + static_cast<int>(status) + ")";
  }
}

int hexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

bool parseBssid(const char* text, uint8_t out[6]) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }

  uint8_t bytes[6] = {0};
  size_t byteIndex = 0;
  const char* p = text;
  while (*p != '\0' && byteIndex < 6) {
    while (*p == ':' || *p == '-' || *p == ' ') {
      ++p;
    }
    const int high = hexNibble(*p++);
    const int low = hexNibble(*p++);
    if (high < 0 || low < 0) {
      return false;
    }
    bytes[byteIndex++] = static_cast<uint8_t>((high << 4) | low);
    if (*p == ':' || *p == '-') {
      ++p;
    }
  }

  if (byteIndex != 6) {
    return false;
  }
  while (*p == ' ') {
    ++p;
  }
  if (*p != '\0') {
    return false;
  }

  memcpy(out, bytes, sizeof(bytes));
  return true;
}
}  // namespace

void GrWifi::begin() {
  if (!_eventsRegistered) {
    WiFi.onEvent(
        [this](arduino_event_id_t event, arduino_event_info_t info) {
          handleWifiEvent(event, info);
        });
    _eventsRegistered = true;
  }
  // Profiles are owned by ProfileStore. Keep the ESP-IDF station config in
  // RAM so a camera AP restart cannot revive stale persistent fast-connect
  // security state from an earlier WLAN session.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  // BLE + Wi-Fi coexistence on ESP32-S3 requires modem sleep. Disabling it
  // after BLE has been started can abort inside the Wi-Fi stack.
  WiFi.setSleep(true);
  // The application owns retry, backoff and camera WLAN lifecycle. Arduino's
  // background reconnect would reuse the previous BSSID/channel while the
  // application is deliberately relaxing those constraints.
  WiFi.setAutoReconnect(false);
  _lastStatus = WiFi.status();
}

bool GrWifi::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
  return connect(ssid, password, nullptr, timeoutMs, nullptr);
}

bool GrWifi::connect(const char* ssid, const char* password, const char* bssid, uint32_t timeoutMs) {
  return connect(ssid, password, bssid, 0, timeoutMs, nullptr);
}

bool GrWifi::connect(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs) {
  return connect(ssid, password, bssid, channel, timeoutMs, nullptr);
}

bool GrWifi::connect(const char* ssid, const char* password, uint32_t timeoutMs, ConnectGuard guard) {
  return connect(ssid, password, nullptr, 0, timeoutMs, guard);
}

bool GrWifi::connect(const char* ssid, const char* password, const char* bssid, uint32_t timeoutMs, ConnectGuard guard) {
  return connect(ssid, password, bssid, 0, timeoutMs, guard);
}

bool GrWifi::connect(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs, ConnectGuard guard) {
  if (guard != nullptr && !guard()) {
    _lastStatus = WiFi.status();
    return false;
  }

  if (isConnected() && ssid != nullptr && WiFi.SSID() == ssid) {
    return true;
  }

  return connectTo(ssid, password, bssid, channel, timeoutMs, guard);
}

bool GrWifi::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

void GrWifi::disconnect() {
  WiFi.disconnect(false, false);
  _lastStatus = WiFi.status();
}

String GrWifi::ssid() const {
  return isConnected() ? WiFi.SSID() : String();
}

String GrWifi::bssidString() const {
  return isConnected() ? WiFi.BSSIDstr() : String();
}

uint8_t GrWifi::channel() const {
  return isConnected() ? static_cast<uint8_t>(WiFi.channel()) : 0;
}

int32_t GrWifi::rssi() const {
  return isConnected() ? WiFi.RSSI() : 0;
}

String GrWifi::localIPString() const {
  return isConnected() ? WiFi.localIP().toString() : String("-");
}

String GrWifi::statusText() const {
  return wifiStatusToString(WiFi.status());
}

int GrWifi::lastDisconnectReason() const {
  return _lastDisconnectReason.load();
}

int GrWifi::lastDisconnectRssi() const {
  return _lastDisconnectRssi.load();
}

String GrWifi::lastDisconnectText() const {
  const int reason = lastDisconnectReason();
  if (reason <= 0) {
    return "none";
  }
  const char* name = WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason));
  return String(reason) + "(" + (name != nullptr ? name : "UNKNOWN") + ")";
}

uint32_t GrWifi::disconnectSequence() const {
  return _disconnectSequence.load();
}

bool GrWifi::connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs) {
  return connectTo(ssid, password, bssid, channel, timeoutMs, nullptr);
}

bool GrWifi::connectTo(const char* ssid, const char* password, const char* bssid, uint8_t channel, uint32_t timeoutMs, ConnectGuard guard) {
  if (ssid == nullptr || ssid[0] == '\0') {
    _lastStatus = WL_NO_SSID_AVAIL;
    return false;
  }

  if (guard != nullptr && !guard()) {
    _lastStatus = WiFi.status();
    return false;
  }

  uint8_t parsedBssid[6] = {0};
  const bool hasBssid = parseBssid(bssid, parsedBssid);
  const uint8_t* bssidPtr = hasBssid ? parsedBssid : nullptr;
  const uint32_t operationStartedAt = millis();
  bool securityContextReset = false;
  uint8_t noApRetryCount = 0;

  while (millis() - operationStartedAt < timeoutMs) {
    WiFi.disconnect(false, false);
    delay(100);

    if (guard != nullptr && !guard()) {
      _lastStatus = WiFi.status();
      return false;
    }
    if (millis() - operationStartedAt >= timeoutMs) {
      break;
    }

    _staConnectedAtMs.store(0);
    _gotIpAtMs.store(0);
    _eventChannel.store(0);
    _lastDisconnectReason.store(0);
    _lastDisconnectRssi.store(0);
    const uint32_t disconnectSequenceAtStart = _disconnectSequence.load();

    wl_status_t beginStatus = WL_IDLE_STATUS;
    if (password != nullptr && password[0] != '\0') {
      beginStatus = WiFi.begin(ssid, password, channel, bssidPtr);
    } else {
      beginStatus = WiFi.begin(ssid, nullptr, channel, bssidPtr);
    }

    const uint32_t attemptStartedAt = millis();
    uint32_t loggedDisconnectSequence = disconnectSequenceAtStart;
    bool retryConnection = false;
    Serial.printf("WiFi: begin channel=%u has_bssid=%d remaining_timeout=%lums begin_status=%s\n",
                  static_cast<unsigned>(channel),
                  hasBssid ? 1 : 0,
                  static_cast<unsigned long>(timeoutMs - (attemptStartedAt - operationStartedAt)),
                  wifiStatusToString(beginStatus).c_str());
    while (millis() - operationStartedAt < timeoutMs) {
      if (guard != nullptr && !guard()) {
        WiFi.disconnect(false, false);
        _lastStatus = WiFi.status();
        return false;
      }

      const wl_status_t status = WiFi.status();
      _lastStatus = status;
      const uint32_t currentDisconnectSequence = _disconnectSequence.load();
      if (currentDisconnectSequence != loggedDisconnectSequence) {
        loggedDisconnectSequence = currentDisconnectSequence;
        const int reason = lastDisconnectReason();
        Serial.printf("WiFi: disconnected reason=%s rssi=%d elapsed=%lums\n",
                      lastDisconnectText().c_str(),
                      lastDisconnectRssi(),
                      static_cast<unsigned long>(millis() - attemptStartedAt));
        if (wifiDisconnectReasonRequiresSecurityReset(reason) && !securityContextReset) {
          securityContextReset = true;
          retryConnection = true;
          Serial.println("WiFi: invalid PMKID; resetting RAM-only STA security context and retrying");
          WiFi.disconnect(true, true);
          delay(50);
          WiFi.mode(WIFI_STA);
          WiFi.setSleep(true);
          WiFi.setAutoReconnect(false);
          break;
        }
        if (wifiDisconnectReasonRequiresImmediateRetry(reason) &&
            noApRetryCount < WIFI_NO_AP_RETRIES_WITHIN_BUDGET) {
          ++noApRetryCount;
          retryConnection = true;
          Serial.printf("WiFi: AP not found; retrying scan in same budget retry=%u/%u\n",
                        static_cast<unsigned>(noApRetryCount),
                        static_cast<unsigned>(WIFI_NO_AP_RETRIES_WITHIN_BUDGET));
          break;
        }
      }
      if (status == WL_CONNECTED) {
        const uint32_t staConnectedAtMs = _staConnectedAtMs.load();
        const uint32_t gotIpAtMs = _gotIpAtMs.load();
        const uint32_t associationMs = staConnectedAtMs >= attemptStartedAt
                                         ? staConnectedAtMs - attemptStartedAt
                                         : 0;
        const uint32_t gotIpMs = gotIpAtMs >= attemptStartedAt
                                   ? gotIpAtMs - attemptStartedAt
                                   : millis() - attemptStartedAt;
        Serial.printf("WiFi: connect completed in %lums total=%lums assoc=%lums got_ip=%lums requested_channel=%u actual_channel=%u status=%s security_reset=%d\n",
                      static_cast<unsigned long>(millis() - attemptStartedAt),
                      static_cast<unsigned long>(millis() - operationStartedAt),
                      static_cast<unsigned long>(associationMs),
                      static_cast<unsigned long>(gotIpMs),
                      static_cast<unsigned>(channel),
                      static_cast<unsigned>(_eventChannel.load()),
                      wifiStatusToString(status).c_str(),
                      securityContextReset ? 1 : 0);
        return true;
      }
      yield();
      delay(100);
    }

    if (!retryConnection) {
      break;
    }
  }

  _lastStatus = WiFi.status();
  Serial.printf("WiFi: connect timed out in %lums channel=%u has_bssid=%d status=%s reason=%s rssi=%d security_reset=%d no_ap_retries=%u\n",
                static_cast<unsigned long>(millis() - operationStartedAt),
                static_cast<unsigned>(channel),
                hasBssid ? 1 : 0,
                wifiStatusToString(_lastStatus).c_str(),
                lastDisconnectText().c_str(),
                lastDisconnectRssi(),
                securityContextReset ? 1 : 0,
                static_cast<unsigned>(noApRetryCount));
  return false;
}

void GrWifi::handleWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  const uint32_t nowMs = millis();
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      _staConnectedAtMs.store(nowMs);
      _eventChannel.store(info.wifi_sta_connected.channel);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      _gotIpAtMs.store(nowMs);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      _lastDisconnectReason.store(info.wifi_sta_disconnected.reason);
      _lastDisconnectRssi.store(info.wifi_sta_disconnected.rssi);
      _disconnectSequence.fetch_add(1);
      _staConnectedAtMs.store(0);
      _gotIpAtMs.store(0);
      _eventChannel.store(0);
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      _gotIpAtMs.store(0);
      break;
    default:
      break;
  }
}
