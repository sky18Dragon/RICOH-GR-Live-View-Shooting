#include "gr_wifi.h"

#include <atomic>
#include <cstring>

#include <esp_wifi.h>

#include "config.h"

namespace {
std::atomic<uint32_t> g_wifiDisconnectCount{0};
std::atomic<int> g_wifiLastDisconnectReason{0};
std::atomic<bool> g_wifiStaAssociated{false};
bool g_wifiEventRegistered = false;

void onWifiStaConnected(arduino_event_id_t, arduino_event_info_t) {
  // Associated with the AP; WL_CONNECTED still waits for DHCP.
  g_wifiStaAssociated.store(true);
}

void onWifiStaDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  const int reason = static_cast<int>(info.wifi_sta_disconnected.reason);
  g_wifiStaAssociated.store(false);
  g_wifiLastDisconnectReason.store(reason);
  g_wifiDisconnectCount.fetch_add(1);
  Serial.printf("WiFi: STA disconnected reason=%d (%s)\n",
                reason,
                GrWifi::disconnectReasonName(reason));
}

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
  if (!g_wifiEventRegistered) {
    WiFi.onEvent(onWifiStaConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(onWifiStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    g_wifiEventRegistered = true;
  }
  applyLinkTuning("begin");
  _lastStatus = WiFi.status();
}

void GrWifi::applyLinkTuning(const char* reason) {
  // MIN_MODEM wakes for every DTIM beacon; MAX_MODEM would sleep through
  // listen intervals and starve the MJPEG stream. NONE is not allowed while
  // BLE is enabled.
  const esp_err_t psErr = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  // RICOH camera APs run 2.4 GHz; a 40 MHz STA channel only widens the
  // window that coexistence and neighbouring networks can disturb.
  const esp_err_t bwErr = esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  const bool txOk = WiFi.setTxPower(WIFI_POWER_19_5dBm);
  Serial.printf("WiFi tune (%s): ps=MIN_MODEM rc=%d bw=HT20 rc=%d tx=%.2fdBm set=%d\n",
                reason != nullptr ? reason : "",
                static_cast<int>(psErr),
                static_cast<int>(bwErr),
                txPowerDbm(),
                txOk ? 1 : 0);
}

uint32_t GrWifi::disconnectCount() const {
  return g_wifiDisconnectCount.load();
}

int GrWifi::lastDisconnectReason() const {
  return g_wifiLastDisconnectReason.load();
}

float GrWifi::txPowerDbm() const {
  int8_t quarterDbm = 0;
  if (esp_wifi_get_max_tx_power(&quarterDbm) != ESP_OK) {
    return -1.0f;
  }
  return quarterDbm * 0.25f;
}

const char* GrWifi::disconnectReasonName(int reason) {
  switch (reason) {
    case 0:
      return "-";
    case WIFI_REASON_AUTH_EXPIRE:
      return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:
      return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:
      return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_LEAVE:
      return "ASSOC_LEAVE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:
      return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:
      return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
      return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
      return "CONNECTION_FAIL";
    default:
      return "OTHER";
  }
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

  // GR II provisioning and other mode switches can restart the STA
  // interface, which drops these settings; reapply before every join.
  applyLinkTuning("connect");

  uint8_t parsedBssid[6] = {0};
  const bool hasBssid = parseBssid(bssid, parsedBssid);
  const uint8_t* bssidPtr = hasBssid ? parsedBssid : nullptr;

  const char* beginPassword = (password != nullptr && password[0] != '\0') ? password : nullptr;
  g_wifiStaAssociated.store(false);
  WiFi.begin(ssid, beginPassword, channel, bssidPtr);

  const uint32_t startMs = millis();
  uint32_t lastBeginMs = startMs;
  uint8_t kicks = 0;
  Serial.printf("WiFi: begin channel=%u has_bssid=%d timeout=%lums\n",
                static_cast<unsigned>(channel),
                hasBssid ? 1 : 0,
                static_cast<unsigned long>(timeoutMs));
  while (millis() - startMs < timeoutMs) {
    if (guard != nullptr && !guard()) {
      WiFi.disconnect(false, false);
      _lastStatus = WiFi.status();
      Serial.printf("WiFi: connect cancelled by guard after %lums channel=%u\n",
                    static_cast<unsigned long>(millis() - startMs),
                    static_cast<unsigned>(channel));
      return false;
    }

    const wl_status_t status = WiFi.status();
    _lastStatus = status;
    if (status == WL_CONNECTED) {
      Serial.printf("WiFi: connect completed in %lums channel=%u status=%s kicks=%u\n",
                    static_cast<unsigned long>(millis() - startMs),
                    static_cast<unsigned>(channel),
                    wifiStatusToString(status).c_str(),
                    static_cast<unsigned>(kicks));
      return true;
    }

    const uint32_t nowMs = millis();
    const uint32_t elapsedMs = nowMs - startMs;
    if (!g_wifiStaAssociated.load() &&
        kicks < WIFI_CONNECT_MAX_KICKS &&
        (nowMs - lastBeginMs) >= WIFI_CONNECT_KICK_MS &&
        elapsedMs + WIFI_CONNECT_KICK_MIN_REMAINING_MS <= timeoutMs) {
      ++kicks;
      Serial.printf("WiFi: not associated after %lums; re-issuing join (kick %u/%u)\n",
                    static_cast<unsigned long>(nowMs - lastBeginMs),
                    static_cast<unsigned>(kicks),
                    static_cast<unsigned>(WIFI_CONNECT_MAX_KICKS));
      WiFi.disconnect(false, false);
      delay(20);
      g_wifiStaAssociated.store(false);
      WiFi.begin(ssid, beginPassword, channel, bssidPtr);
      lastBeginMs = millis();
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
