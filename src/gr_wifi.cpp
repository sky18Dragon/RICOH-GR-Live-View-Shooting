#include "gr_wifi.h"

#include <cstring>

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
  WiFi.mode(WIFI_STA);
  // BLE + Wi-Fi coexistence on ESP32-S3 requires modem sleep. Disabling it
  // after BLE has been started can abort inside the Wi-Fi stack.
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
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

int32_t GrWifi::rssi() const {
  return isConnected() ? WiFi.RSSI() : 0;
}

String GrWifi::localIPString() const {
  return isConnected() ? WiFi.localIP().toString() : String("-");
}

String GrWifi::statusText() const {
  return wifiStatusToString(WiFi.status());
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

  WiFi.disconnect(false, false);
  delay(100);

  if (guard != nullptr && !guard()) {
    _lastStatus = WiFi.status();
    return false;
  }

  uint8_t parsedBssid[6] = {0};
  const bool hasBssid = parseBssid(bssid, parsedBssid);
  const uint8_t* bssidPtr = hasBssid ? parsedBssid : nullptr;

  if (password != nullptr && password[0] != '\0') {
    WiFi.begin(ssid, password, channel, bssidPtr);
  } else {
    WiFi.begin(ssid, nullptr, channel, bssidPtr);
  }

  const uint32_t startMs = millis();
  Serial.printf("WiFi: begin channel=%u has_bssid=%d timeout=%lums\n",
                static_cast<unsigned>(channel),
                hasBssid ? 1 : 0,
                static_cast<unsigned long>(timeoutMs));
  while (millis() - startMs < timeoutMs) {
    if (guard != nullptr && !guard()) {
      WiFi.disconnect(false, false);
      _lastStatus = WiFi.status();
      return false;
    }

    const wl_status_t status = WiFi.status();
    _lastStatus = status;
    if (status == WL_CONNECTED) {
      Serial.printf("WiFi: connect completed in %lums channel=%u status=%s\n",
                    static_cast<unsigned long>(millis() - startMs),
                    static_cast<unsigned>(channel),
                    wifiStatusToString(status).c_str());
      return true;
    }
    yield();
    delay(100);
  }

  _lastStatus = WiFi.status();
  Serial.printf("WiFi: connect timed out in %lums channel=%u status=%s\n",
                static_cast<unsigned long>(millis() - startMs),
                static_cast<unsigned>(channel),
                wifiStatusToString(_lastStatus).c_str());
  return false;
}
