#include "ricoh_ble_client.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include <NimBLEAdvertisedDevice.h>
#include <NimBLEClient.h>
#include <NimBLEConnInfo.h>
#include <NimBLEDevice.h>
#include <NimBLERemoteCharacteristic.h>
#include <NimBLERemoteService.h>
#include <NimBLEScan.h>
#include <NimBLEUUID.h>
#include <esp_heap_caps.h>

extern "C" {
#ifdef USING_NIMBLE_ARDUINO_HEADERS
#include "nimble/nimble/host/include/host/ble_gap.h"
#include "nimble/nimble/host/include/host/ble_gatt.h"
#include "nimble/nimble/host/include/host/ble_hs.h"
#else
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#endif
}

#include "config.h"

namespace {
constexpr uint32_t HANDLE_WRITE_TIMEOUT_MS = 3000;
constexpr uint32_t HANDLE_READ_TIMEOUT_MS = 800;
constexpr uint32_t BLE_SCAN_SLICE_MS = 250;
std::atomic<int> g_lastDisconnectReason{0};
std::atomic<int> g_powerOffDisconnectReason{0};
std::atomic<int> g_powerStateNotifyValue{-1};
std::atomic<bool> g_powerOffNotifyPending{false};
RicohBleClient::ServiceCallback g_serviceCallback = nullptr;

constexpr uint8_t RICOH_SHOOTING_FLAVOR_IMMEDIATE = 0x00;
constexpr uint8_t RICOH_OPERATION_START = 0x01;
constexpr uint8_t RICOH_OPERATION_PARAM_NO_AF = 0x00;
constexpr uint8_t RICOH_OPERATION_PARAM_AF = 0x01;

bool isPowerOffDisconnectReason(int reason) {
  return reason == RICOH_BLE_DISCONNECT_REMOTE_USER ||
         reason == RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF;
}

const char* ricohOperationModeName(RicohCameraOperationMode mode) {
  switch (mode) {
    case RicohCameraOperationMode::Capture:
      return "CAPTURE";
    case RicohCameraOperationMode::Playback:
      return "PLAYBACK";
    case RicohCameraOperationMode::BleStartup:
      return "BLE_STARTUP";
    case RicohCameraOperationMode::Other:
      return "OTHER";
    case RicohCameraOperationMode::PowerOffTransfer:
      return "POWER_OFF_TRANSFER";
    case RicohCameraOperationMode::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

int ricohGapEventHandler(ble_gap_event* event, void*) {
  if (event == nullptr || event->type != BLE_GAP_EVENT_NOTIFY_RX ||
      event->notify_rx.attr_handle != RICOH_BLE_GR4_POWER_STATE_HANDLE ||
      event->notify_rx.om == nullptr || OS_MBUF_PKTLEN(event->notify_rx.om) < 1) {
    return 0;
  }

  uint8_t value = 0xFF;
  if (os_mbuf_copydata(event->notify_rx.om, 0, sizeof(value), &value) != 0) {
    return 0;
  }

  g_powerStateNotifyValue.store(value);
  Serial.printf("BLE: power notify handle=0x%04X value=0x%02X\n",
                RICOH_BLE_GR4_POWER_STATE_HANDLE,
                value);
  if (value == RICOH_BLE_GR4_POWER_STATE_OFF_VALUE) {
    g_powerOffNotifyPending.store(true);
  } else if (value == RICOH_BLE_GR4_POWER_STATE_ON_VALUE) {
    g_powerOffNotifyPending.store(false);
  }
  return 0;
}

struct WlanParamHandle {
  uint16_t handle;
  const char* label;
};

const WlanParamHandle kWlanParamHandles[] = {
    {RICOH_BLE_GR4_WLAN_SSID_HANDLE, "ssid"},
    {RICOH_BLE_GR4_WLAN_PASSPHRASE_HANDLE, "passphrase"},
    {RICOH_BLE_GR4_WLAN_SECURITY_HANDLE, "security"},
    {RICOH_BLE_GR4_WLAN_FREQUENCY_HANDLE, "frequency"},
    {RICOH_BLE_GR4_WLAN_BSSID_HANDLE, "bssid"},
};

String toUpperCopy(String value) {
  value.toUpperCase();
  return value;
}

bool nameLooksLikeRicoh(const String& name) {
  const String upper = toUpperCopy(name);
  return upper == "GR" || upper.startsWith("GR_") || upper.indexOf("RICOH") >= 0 ||
         upper.indexOf("PENTAX") >= 0 || upper.indexOf("GR ") >= 0 ||
         upper.indexOf("GRIII") >= 0 || upper.indexOf("GR III") >= 0;
}

bool addressMatches(const String& candidate, const String& preferred) {
  return preferred.length() > 0 && candidate.equalsIgnoreCase(preferred);
}

bool nameMatchesPreferred(const String& candidate, const String& preferredName) {
  return preferredName.length() > 0 && candidate.length() > 0 && candidate.equalsIgnoreCase(preferredName);
}

bool advertisesAnyRicohService(const NimBLEAdvertisedDevice* device) {
  if (device == nullptr) {
    return false;
  }
  return device->isAdvertisingService(NimBLEUUID(RICOH_BLE_INFO_SERVICE_UUID)) ||
         device->isAdvertisingService(NimBLEUUID(RICOH_BLE_CAMERA_SERVICE_UUID)) ||
         device->isAdvertisingService(NimBLEUUID(RICOH_BLE_SHOOTING_SERVICE_UUID)) ||
         device->isAdvertisingService(NimBLEUUID(RICOH_BLE_CONTROL_SERVICE_UUID));
}

bool hasRicohIdentitySignal(const RicohBleDeviceInfo& info) {
  return info.name.length() > 0 ||
         info.hasInfoService ||
         info.hasCameraService ||
         info.hasShootingService ||
         info.hasControlService;
}

RicohBleDeviceInfo infoFromAdvertisedDevice(const NimBLEAdvertisedDevice* device) {
  RicohBleDeviceInfo info;
  if (device == nullptr) {
    return info;
  }

  info.found = true;
  info.address = device->getAddress().toString().c_str();
  info.addressType = device->getAddressType();
  info.rssi = device->getRSSI();
  info.connectable = device->isConnectable();
  if (device->haveName()) {
    info.name = device->getName().c_str();
  }
  info.hasInfoService = device->isAdvertisingService(NimBLEUUID(RICOH_BLE_INFO_SERVICE_UUID));
  info.hasCameraService = device->isAdvertisingService(NimBLEUUID(RICOH_BLE_CAMERA_SERVICE_UUID));
  info.hasShootingService = device->isAdvertisingService(NimBLEUUID(RICOH_BLE_SHOOTING_SERVICE_UUID));
  info.hasControlService = device->isAdvertisingService(NimBLEUUID(RICOH_BLE_CONTROL_SERVICE_UUID));
  return info;
}

int candidateScore(const RicohBleDeviceInfo& info, const String& preferredAddress, const String& preferredName) {
  int score = 0;
  if (info.connectable) {
    score += 2000;
  }
  if (nameMatchesPreferred(info.name, preferredName)) {
    score += 1200;
  } else if (addressMatches(info.address, preferredAddress)) {
    score += 500;
  }
  if (info.hasShootingService) {
    score += 300;
  }
  if (info.hasCameraService) {
    score += 250;
  }
  if (info.hasInfoService) {
    score += 200;
  }
  if (info.hasControlService) {
    score += 150;
  }
  if (nameLooksLikeRicoh(info.name)) {
    score += 50;
  }
  score += info.rssi;
  return score;
}

bool isRicohCandidate(const NimBLEAdvertisedDevice* device,
                      const RicohBleDeviceInfo& info,
                      const String& preferredAddress,
                      const String& preferredName) {
  const bool serviceMatch = advertisesAnyRicohService(device);
  const bool nameMatch = nameLooksLikeRicoh(info.name);
  const bool preferredMatch = addressMatches(info.address, preferredAddress) || nameMatchesPreferred(info.name, preferredName);
  return serviceMatch || nameMatch || (preferredMatch && info.connectable);
}

class RicohScanCallbacks : public NimBLEScanCallbacks {
public:
  RicohScanCallbacks(const String& preferredAddress, const String& preferredName)
      : _preferredAddress(preferredAddress), _preferredName(preferredName) {}

  void prepareForScan() {
    _scanEnded.store(false);
  }

  void onDiscovered(const NimBLEAdvertisedDevice* device) override {
    RicohBleDeviceInfo info = infoFromAdvertisedDevice(device);
    if (addressMatches(info.address, _preferredAddress) && info.connectable) {
      updateBest(info, device);
    }
  }

  void onResult(const NimBLEAdvertisedDevice* device) override {
    RicohBleDeviceInfo info = infoFromAdvertisedDevice(device);
    if (!isRicohCandidate(device, info, _preferredAddress, _preferredName)) {
      return;
    }

    updateBest(info, device);
    if (info.connectable &&
        hasRicohIdentitySignal(info) &&
        (addressMatches(info.address, _preferredAddress) || nameMatchesPreferred(info.name, _preferredName))) {
      _foundPreferred.store(true);
    }
  }

  void onScanEnd(const NimBLEScanResults&, int) override {
    _scanEnded.store(true);
  }

  const RicohBleDeviceInfo& best() const { return _best; }
  bool hasBest() const { return _best.found; }
  bool foundPreferred() const { return _foundPreferred.load(); }
  bool scanEnded() const { return _scanEnded.load(); }

private:
  void updateBest(const RicohBleDeviceInfo& info, const NimBLEAdvertisedDevice* device) {
    const int score = candidateScore(info, _preferredAddress, _preferredName);
    if (!_best.found || score > _bestScore) {
      _best = info;
      _bestScore = score;
    }
  }

  String _preferredAddress;
  String _preferredName;
  RicohBleDeviceInfo _best;
  int _bestScore = -100000;
  std::atomic<bool> _foundPreferred{false};
  std::atomic<bool> _scanEnded{false};
};

String printableText(const std::vector<uint8_t>& data) {
  String text;
  for (uint8_t b : data) {
    if (b == 0) {
      continue;
    }
    if (b == '\r' || b == '\n' || b == '\t') {
      text += ' ';
      continue;
    }
    if (b < 0x20 || b > 0x7E) {
      return String();
    }
    text += static_cast<char>(b);
  }
  text.trim();
  return text;
}

String maybeQuotedValue(const String& text, const char* key) {
  String lowerText = text;
  lowerText.toLowerCase();
  String lowerKey = key;
  lowerKey.toLowerCase();

  const int keyPos = lowerText.indexOf(lowerKey);
  if (keyPos < 0) {
    return String();
  }

  int sep = -1;
  for (int i = keyPos + lowerKey.length(); i < text.length(); ++i) {
    const char c = text[i];
    if (c == ':' || c == '=') {
      sep = i;
      break;
    }
    if (!(c == ' ' || c == '\t' || c == '"' || c == '\'')) {
      break;
    }
  }
  if (sep < 0) {
    return String();
  }

  int start = sep + 1;
  while (start < text.length() && (text[start] == ' ' || text[start] == '\t' || text[start] == '"' || text[start] == '\'')) {
    ++start;
  }

  int end = start;
  while (end < text.length()) {
    const char c = text[end];
    if (c == '"' || c == '\'' || c == ',' || c == ';' || c == '}' || c == '\r' || c == '\n') {
      break;
    }
    ++end;
  }

  String value = text.substring(start, end);
  value.trim();
  return value;
}

bool looksLikeSsid(const String& text) {
  if (text.length() == 0 || text.length() > 32) {
    return false;
  }
  String upper = text;
  upper.toUpperCase();
  return upper.startsWith("GR_") || upper.startsWith("RICOH_GR") ||
         upper.startsWith("GR4_") || upper.startsWith("GR_4_");
}

bool isHexPair(char a, char b) {
  return std::isxdigit(static_cast<unsigned char>(a)) && std::isxdigit(static_cast<unsigned char>(b));
}

String macFromText(const String& text) {
  for (int i = 0; i + 16 < text.length(); ++i) {
    if (isHexPair(text[i], text[i + 1]) &&
        (text[i + 2] == ':' || text[i + 2] == '-') &&
        isHexPair(text[i + 3], text[i + 4]) &&
        text[i + 5] == text[i + 2] &&
        isHexPair(text[i + 6], text[i + 7]) &&
        text[i + 8] == text[i + 2] &&
        isHexPair(text[i + 9], text[i + 10]) &&
        text[i + 11] == text[i + 2] &&
        isHexPair(text[i + 12], text[i + 13]) &&
        text[i + 14] == text[i + 2] &&
        isHexPair(text[i + 15], text[i + 16])) {
      String mac = text.substring(i, i + 17);
      mac.replace('-', ':');
      mac.toUpperCase();
      return mac;
    }
  }
  return String();
}

String macFromRaw6(const std::vector<uint8_t>& data) {
  if (data.size() != 6) {
    return String();
  }
  char mac[18];
  snprintf(mac,
           sizeof(mac),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           data[0],
           data[1],
           data[2],
           data[3],
           data[4],
           data[5]);
  return String(mac);
}

uint16_t frequencyMhzForChannel(uint8_t channel) {
  if (channel >= 1 && channel <= 13) {
    return static_cast<uint16_t>(2407 + channel * 5);
  }
  if (channel == 14) {
    return 2484;
  }
  return 0;
}

uint8_t channelFromFrequencyMhz(uint16_t frequencyMhz) {
  if (frequencyMhz == 2484) {
    return 14;
  }
  if (frequencyMhz >= 2412 && frequencyMhz <= 2472 && ((frequencyMhz - 2407) % 5) == 0) {
    return static_cast<uint8_t>((frequencyMhz - 2407) / 5);
  }
  return 0;
}

uint16_t normalizeFrequencyOrChannel(uint32_t value) {
  if (value >= 2412 && value <= 2484) {
    return static_cast<uint16_t>(value);
  }
  if (value >= 1 && value <= 14) {
    return frequencyMhzForChannel(static_cast<uint8_t>(value));
  }
  return 0;
}

uint16_t frequencyFromText(const String& text) {
  const char* cursor = text.c_str();
  while (*cursor != '\0') {
    while (*cursor != '\0' && !std::isdigit(static_cast<unsigned char>(*cursor))) {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    char* end = nullptr;
    const unsigned long value = std::strtoul(cursor, &end, 10);
    const uint16_t normalized = normalizeFrequencyOrChannel(value);
    if (normalized != 0) {
      return normalized;
    }
    cursor = end != nullptr && end != cursor ? end : cursor + 1;
  }
  return 0;
}

uint32_t readUnsignedLe(const std::vector<uint8_t>& data, size_t width) {
  uint32_t value = 0;
  const size_t count = std::min(width, data.size());
  for (size_t i = 0; i < count; ++i) {
    value |= static_cast<uint32_t>(data[i]) << (8 * i);
  }
  return value;
}

uint32_t readUnsignedBe(const std::vector<uint8_t>& data, size_t width) {
  uint32_t value = 0;
  const size_t count = std::min(width, data.size());
  for (size_t i = 0; i < count; ++i) {
    value = (value << 8) | data[i];
  }
  return value;
}

uint16_t frequencyFromRaw(const std::vector<uint8_t>& data) {
  if (data.empty() || data.size() > 4) {
    return 0;
  }
  const uint16_t le = normalizeFrequencyOrChannel(readUnsignedLe(data, data.size()));
  if (le != 0) {
    return le;
  }
  return normalizeFrequencyOrChannel(readUnsignedBe(data, data.size()));
}

uint16_t frequencyFromValue(const String& text, const std::vector<uint8_t>& data) {
  const uint16_t fromText = frequencyFromText(text);
  if (fromText != 0) {
    return fromText;
  }
  return frequencyFromRaw(data);
}

void logBleHeapOnResourceError(const char* label) {
  Serial.printf("BLE heap: %s free_internal=%u largest_internal=%u free_psram=%u\n",
                label != nullptr ? label : "resource error",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

void mergeCredentialValue(RicohBleWifiCredentials& out, const char* label, const std::vector<uint8_t>& data) {
  if (data.empty()) {
    return;
  }

  const String text = printableText(data);
  if (text.length() > 0) {
    String value = maybeQuotedValue(text, "ssid");
    if (looksLikeSsid(value)) {
      out.ssid = value;
    }

    value = maybeQuotedValue(text, "passphrase");
    if (value.length() == 0) {
      value = maybeQuotedValue(text, "password");
    }
    if (value.length() > 0) {
      out.passphrase = value;
      out.encryptedPassphrase = false;
    }

    value = maybeQuotedValue(text, "bssid");
    if (value.length() == 0) {
      value = macFromText(text);
    }
    if (value.length() > 0) {
      out.bssid = value;
    }
  }

  if (strcmp(label, "ssid") == 0 && out.ssid.length() == 0 && looksLikeSsid(text)) {
    out.ssid = text;
  } else if (strcmp(label, "passphrase") == 0 && out.passphrase.length() == 0) {
    if (text.length() > 0) {
      out.passphrase = text;
      out.encryptedPassphrase = false;
    } else {
      out.encryptedPassphrase = true;
    }
  } else if (strcmp(label, "bssid") == 0 && out.bssid.length() == 0) {
    String mac = text.length() > 0 ? macFromText(text) : String();
    if (mac.length() == 0) {
      mac = macFromRaw6(data);
    }
    if (mac.length() > 0) {
      out.bssid = mac;
    }
  } else if (strcmp(label, "security") == 0 && data.size() == 1) {
    out.securityType = data[0];
  } else if (strcmp(label, "frequency") == 0) {
    const uint16_t frequencyMhz = frequencyFromValue(text, data);
    const uint8_t channel = channelFromFrequencyMhz(frequencyMhz);
    if (frequencyMhz != 0 && channel != 0) {
      out.frequencyMhz = frequencyMhz;
      out.channel = channel;
    }
  }

  out.valid = out.ssid.length() > 0 &&
              !out.encryptedPassphrase &&
              (out.passphrase.length() > 0 || out.securityType == 0);
}

class RicohNimBleCallbacks : public NimBLEClientCallbacks {
public:
  void onConnectFail(NimBLEClient*, int reason) override {
    Serial.printf("BLE: connect failed reason=%d\n", reason);
  }

  void onDisconnect(NimBLEClient*, int reason) override {
    g_lastDisconnectReason.store(reason);
    if (isPowerOffDisconnectReason(reason)) {
      g_powerOffDisconnectReason.store(reason);
    }
    Serial.printf("BLE: disconnected reason=%d\n", reason);
  }

  void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
    Serial.printf("BLE security: passkey requested by %s\n", connInfo.getAddress().toString().c_str());
    NimBLEDevice::injectPassKey(connInfo, 123456);
  }

  uint32_t onPassKeyDisplay(NimBLEConnInfo&) override {
    return 123456;
  }

  void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pin) override {
    Serial.printf("BLE security: confirming passkey %06lu\n", static_cast<unsigned long>(pin));
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    if (!connInfo.isEncrypted()) {
      Serial.println("BLE security: authentication completed without encryption");
    }
  }

};

RicohNimBleCallbacks g_callbacks;

void configureRicohSecurity() {
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT);
}

struct WriteContext {
  volatile bool done = false;
  volatile int status = BLE_HS_EUNKNOWN;
  volatile bool abandoned = false;
};

void finishWriteContext(WriteContext* ctx, int status) {
  if (ctx == nullptr) {
    return;
  }
  ctx->status = status;
  ctx->done = true;
  if (ctx->abandoned) {
    delete ctx;
  }
}

int handleWriteCallback(uint16_t, const ble_gatt_error* error, ble_gatt_attr*, void* arg) {
  finishWriteContext(static_cast<WriteContext*>(arg), error != nullptr ? error->status : BLE_HS_EUNKNOWN);
  return 0;
}

bool writeHandleWithResponse(NimBLEClient* client, uint16_t handle, const uint8_t* data, size_t length, String& errorOut) {
  if (client == nullptr || !client->isConnected()) {
    errorOut = "BLE not connected";
    return false;
  }
  if (data == nullptr || length == 0 || length > UINT16_MAX) {
    errorOut = "Invalid BLE write payload";
    return false;
  }

  WriteContext* ctx = new (std::nothrow) WriteContext();
  if (ctx == nullptr) {
    errorOut = "No memory for BLE write";
    return false;
  }

  const int rc = ble_gattc_write_flat(client->getConnHandle(),
                                      handle,
                                      data,
                                      static_cast<uint16_t>(length),
                                      handleWriteCallback,
                                      ctx);
  if (rc != 0) {
    delete ctx;
    errorOut = String("NimBLE write start failed rc=") + String(rc);
    return false;
  }

  const uint32_t startMs = millis();
  while (!ctx->done && (millis() - startMs) < HANDLE_WRITE_TIMEOUT_MS) {
    delay(10);
    yield();
  }
  if (!ctx->done) {
    ctx->abandoned = true;
    if (client->isConnected()) {
      client->disconnect();
    }
    errorOut = "NimBLE write timeout";
    return false;
  }

  const int status = ctx->status;
  delete ctx;
  if (status != 0) {
    errorOut = String("NimBLE write failed status=") + String(static_cast<int>(status));
    return false;
  }
  errorOut = "";
  return true;
}

struct ReadContext {
  volatile bool done = false;
  volatile int status = BLE_HS_EUNKNOWN;
  volatile bool abandoned = false;
  bool longRead = false;
  std::vector<uint8_t> value;
};

void finishReadContext(ReadContext* ctx, int status) {
  if (ctx == nullptr) {
    return;
  }
  ctx->status = status;
  ctx->done = true;
  if (ctx->abandoned) {
    delete ctx;
  }
}

int handleReadCallback(uint16_t, const ble_gatt_error* error, ble_gatt_attr* attr, void* arg) {
  ReadContext* ctx = static_cast<ReadContext*>(arg);
  if (ctx == nullptr) {
    return 0;
  }

  const int status = error != nullptr ? error->status : BLE_HS_EUNKNOWN;
  if (status == 0 && attr != nullptr && attr->om != nullptr) {
    const uint16_t len = OS_MBUF_PKTLEN(attr->om);
    const size_t oldSize = ctx->value.size();
    ctx->value.resize(oldSize + len);
    if (len > 0) {
      os_mbuf_copydata(attr->om, 0, len, ctx->value.data() + oldSize);
    }
    if (!ctx->longRead) {
      finishReadContext(ctx, 0);
    }
    return 0;
  }

  finishReadContext(ctx, status);
  return 0;
}

bool readHandleOnce(NimBLEClient* client, uint16_t handle, bool longRead, std::vector<uint8_t>& value, String& errorOut) {
  if (client == nullptr || !client->isConnected()) {
    errorOut = "BLE not connected";
    return false;
  }

  value.clear();
  ReadContext* ctx = new (std::nothrow) ReadContext();
  if (ctx == nullptr) {
    errorOut = "No memory for BLE read";
    return false;
  }

  ctx->longRead = longRead;
  const int rc = longRead
                   ? ble_gattc_read_long(client->getConnHandle(), handle, 0, handleReadCallback, ctx)
                   : ble_gattc_read(client->getConnHandle(), handle, handleReadCallback, ctx);
  if (rc != 0) {
    delete ctx;
    errorOut = String("NimBLE read start failed rc=") + String(rc);
    return false;
  }

  const uint32_t startMs = millis();
  while (!ctx->done && (millis() - startMs) < HANDLE_READ_TIMEOUT_MS) {
    delay(10);
    yield();
  }
  if (!ctx->done) {
    ctx->abandoned = true;
    if (client->isConnected()) {
      client->disconnect();
    }
    errorOut = "NimBLE read timeout";
    return false;
  }

  const int status = ctx->status;
  if (status == 0 || status == BLE_HS_EDONE) {
    value = ctx->value;
    delete ctx;
    errorOut = "";
    return true;
  }

  delete ctx;
  errorOut = String("NimBLE read failed status=") + String(static_cast<int>(status));
  return false;
}

bool readHandleWithResponse(NimBLEClient* client, uint16_t handle, std::vector<uint8_t>& value, String& errorOut) {
  if (readHandleOnce(client, handle, true, value, errorOut)) {
    return true;
  }

  const bool attrNotLong = errorOut.indexOf(String(static_cast<int>(BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG)))) >= 0;
  if (!attrNotLong) {
    return false;
  }

  return readHandleOnce(client, handle, false, value, errorOut);
}

NimBLERemoteCharacteristic* writableCharacteristic(NimBLERemoteService* service,
                                                   const char* uuid,
                                                   const char* label,
                                                   String& errorOut) {
  if (service == nullptr) {
    errorOut = "BLE shooting service unavailable";
    return nullptr;
  }

  NimBLERemoteCharacteristic* characteristic = service->getCharacteristic(NimBLEUUID(uuid));
  if (characteristic == nullptr || !characteristic->canWrite()) {
    errorOut = String("BLE ") + label + " unavailable";
    return nullptr;
  }
  errorOut = "";
  return characteristic;
}

bool writeCharacteristicValue(NimBLERemoteCharacteristic* characteristic,
                              const uint8_t* payload,
                              size_t length,
                              const char* label,
                              String& errorOut) {
  if (characteristic == nullptr || payload == nullptr || length == 0) {
    errorOut = String("BLE ") + label + " invalid write";
    return false;
  }

  if (!characteristic->writeValue(payload, length, true)) {
    errorOut = String("BLE ") + label + " write failed";
    return false;
  }

  errorOut = "";
  return true;
}

bool waitForEncryptedConnection(NimBLEClient* client, uint32_t timeoutMs, String& errorOut) {
  if (client == nullptr || !client->isConnected()) {
    errorOut = "BLE not connected";
    return false;
  }

  const uint32_t startMs = millis();
  while (client->isConnected() && (millis() - startMs) < timeoutMs) {
    if (g_serviceCallback != nullptr && g_serviceCallback()) {
      errorOut = "BLE operation aborted";
      return false;
    }
    NimBLEConnInfo info = client->getConnInfo();
    if (info.isEncrypted()) {
      errorOut = "";
      return true;
    }
    delay(50);
    yield();
  }

  errorOut = client->isConnected() ? "BLE security timeout" : "BLE lost during security";
  return false;
}
}  // namespace

void RicohBleClient::begin() {
  if (_begun) {
    return;
  }

  NimBLEDevice::init("RICOH-StickS3");
  NimBLEDevice::setPowerLevel(ESP_PWR_LVL_P9);
  configureRicohSecurity();
  NimBLEDevice::setCustomGapHandler(ricohGapEventHandler);
  _begun = true;
  _lastError = "";
  Serial.printf("BLE: NimBLE initialized (%s)\n", NimBLEDevice::getVersion());
}

void RicohBleClient::setServiceCallback(ServiceCallback callback) {
  g_serviceCallback = callback;
}

RicohBleDeviceInfo RicohBleClient::scanForCamera(const String& preferredAddress,
                                                 const String& preferredName,
                                                 uint32_t scanSeconds) {
  begin();

  NimBLEScan* scan = NimBLEDevice::getScan();
  RicohScanCallbacks callbacks(preferredAddress, preferredName);
  scan->setScanCallbacks(&callbacks, false);
  scan->setActiveScan(true);
  scan->setInterval(80);
  scan->setWindow(79);
  scan->setScanResponseTimeout(150);
  scan->setMaxResults(0);

  const uint32_t durationMs = scanSeconds * 1000;
  Serial.printf("BLE: scanning for GR camera (%lus max)\n", static_cast<unsigned long>(scanSeconds));
  const uint32_t startMs = millis();
  uint32_t remainingMs = durationMs;
  bool aborted = false;
  bool startFailed = false;

  // NimBLE dispatches scan callbacks on its host task. Stopping and clearing
  // results from this task can delete an advertised device while onResult()
  // is still parsing it. Use short blocking scan slices instead: getResults()
  // returns only after onScanEnd(), so the callback and result lifetimes are
  // quiescent before this task reads them or starts the next slice.
  while (remainingMs > 0 && !callbacks.foundPreferred()) {
    if (g_serviceCallback != nullptr && g_serviceCallback()) {
      aborted = true;
      break;
    }

    const uint32_t sliceMs = std::min(remainingMs, BLE_SCAN_SLICE_MS);
    callbacks.prepareForScan();
    (void)scan->getResults(sliceMs, false);
    if (!callbacks.scanEnded()) {
      startFailed = true;
      break;
    }

    remainingMs -= sliceMs;
    if (g_serviceCallback != nullptr && g_serviceCallback()) {
      aborted = true;
      break;
    }
    yield();
  }

  RicohBleDeviceInfo best = callbacks.best();
  scan->setScanCallbacks(nullptr, false);
  scan->clearResults();

  if (aborted) {
    _lastError = "BLE scan aborted";
    return RicohBleDeviceInfo{};
  }
  if (startFailed) {
    _lastError = "BLE scan start failed";
    return RicohBleDeviceInfo{};
  }
  if (!best.found) {
    _lastError = "RICOH BLE not found";
  } else {
    _lastError = "";
    Serial.printf("BLE: selected camera name='%s' addr=%s rssi=%d connectable=%d scan_ms=%lu%s\n",
                  best.name.c_str(),
                  best.address.c_str(),
                  best.rssi,
                  best.connectable ? 1 : 0,
                  static_cast<unsigned long>(millis() - startMs),
                  callbacks.foundPreferred() ? " preferred" : "");
  }
  return best;
}

bool RicohBleClient::connect(const RicohBleDeviceInfo& info, uint32_t timeoutMs) {
  RicohBleConnectOptions options;
  options.timeoutMs = timeoutMs;
  options.securityWaitMs = RICOH_BLE_SECURITY_WAIT_MS;
  options.preConnectDelayMs = BLE_SCAN_TO_CONNECT_DELAY_MS;
  options.exchangeMtu = true;
  return connect(info, options);
}

bool RicohBleClient::connect(const RicohBleDeviceInfo& info, const RicohBleConnectOptions& options) {
  begin();
  _lastFailureResourceExhausted = false;
  const uint32_t connectStartMs = millis();
  disconnect();
  if (!info.found || info.address.length() == 0) {
    _lastError = "No BLE device selected";
    return false;
  }

  if (options.preConnectDelayMs > 0) {
    delay(options.preConnectDelayMs);
    yield();
  }

  Serial.printf("BLE: connect start addr=%s type=%u timeout=%lums mtu=%d pre_delay=%lums\n",
                info.address.c_str(),
                static_cast<unsigned>(info.addressType),
                static_cast<unsigned long>(options.timeoutMs),
                options.exchangeMtu ? 1 : 0,
                static_cast<unsigned long>(options.preConnectDelayMs));

  NimBLEAddress peer(std::string(info.address.c_str()), info.addressType);
  NimBLEClient* client = NimBLEDevice::createClient();
  if (client == nullptr) {
    _lastError = "NimBLE create client failed";
    return false;
  }

  _client = client;
  client->setClientCallbacks(&g_callbacks, false);
  client->setConnectTimeout(options.timeoutMs);
  client->setConnectRetries(1);
  client->setConnectionParams(BLE_GAP_INITIAL_CONN_ITVL_MIN,
                              BLE_GAP_INITIAL_CONN_ITVL_MAX,
                              1,
                              2 * BLE_GAP_INITIAL_SUPERVISION_TIMEOUT);

  if (!client->connect(peer, true, false, options.exchangeMtu)) {
    const int err = client->getLastError();
    _lastFailureResourceExhausted = (err == BLE_HS_ENOMEM);
    _lastError = String("NimBLE connect failed err=") + String(err);
    Serial.printf("BLE: connect failed err=%d elapsed=%lums\n",
                  err,
                  static_cast<unsigned long>(millis() - connectStartMs));
    if (_lastFailureResourceExhausted) {
      logBleHeapOnResourceError("connect");
    }
    NimBLEDevice::deleteClient(client);
    _client = nullptr;
    _connected = false;
    return false;
  }

  _connected = true;
  NimBLEDevice::setPowerLevel(ESP_PWR_LVL_P9);

  const uint32_t securityStartMs = millis();
  const bool alreadyEncrypted = client->getConnInfo().isEncrypted();
  bool securityStarted = alreadyEncrypted;
  int securityErr = 0;
  if (!alreadyEncrypted) {
    securityStarted = client->secureConnection(true);
    securityErr = client->getLastError();
  } else {
    Serial.println("BLE: link already encrypted; skipping duplicate security initiation");
  }
  if (!securityStarted && securityErr != BLE_HS_EALREADY) {
    _lastFailureResourceExhausted = (securityErr == BLE_HS_ENOMEM);
    _lastError = String("NimBLE security start failed err=") + String(securityErr);
    Serial.printf("BLE: security start failed err=%d total_ms=%lums\n",
                  securityErr,
                  static_cast<unsigned long>(millis() - connectStartMs));
    if (_lastFailureResourceExhausted) {
      logBleHeapOnResourceError("security");
    }
    disconnect();
    return false;
  }

  String securityWaitError;
  const uint32_t securityWaitMs = options.securityWaitMs > 0 ? options.securityWaitMs : RICOH_BLE_SECURITY_WAIT_MS;
  if (!waitForEncryptedConnection(client, securityWaitMs, securityWaitError)) {
    _lastFailureResourceExhausted = false;
    _lastError = securityWaitError;
    Serial.printf("BLE: security wait failed after %lums total_ms=%lums: %s\n",
                  static_cast<unsigned long>(millis() - securityStartMs),
                  static_cast<unsigned long>(millis() - connectStartMs),
                  securityWaitError.c_str());
    disconnect();
    return false;
  }

  _lastFailureResourceExhausted = false;
  _lastError = "";
  Serial.printf("BLE: connected secure connect_ms=%lu security_ms=%lu total_ms=%lu\n",
                static_cast<unsigned long>(securityStartMs - connectStartMs),
                static_cast<unsigned long>(millis() - securityStartMs),
                static_cast<unsigned long>(millis() - connectStartMs));
  return true;
}

bool RicohBleClient::isBonded(const RicohBleDeviceInfo& info) {
  begin();
  if (info.address.length() == 0) {
    return false;
  }

  NimBLEAddress peer(std::string(info.address.c_str()), info.addressType);
  return NimBLEDevice::isBonded(peer);
}

bool RicohBleClient::isConnected() const {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  return _connected && client != nullptr && client->isConnected();
}

bool RicohBleClient::shutterReady() const {
  return isConnected();
}

bool RicohBleClient::openWifi() {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  const uint8_t payload[] = {RICOH_BLE_GR4_WLAN_ON_VALUE};
  String err;
  if (!writeHandleWithResponse(client, RICOH_BLE_GR4_WLAN_POWER_HANDLE, payload, sizeof(payload), err)) {
    _lastError = err;
    return false;
  }

  _lastError = "";
  Serial.println("BLE: Wi-Fi open requested");
  return true;
}

bool RicohBleClient::readPowerState(RicohCameraPowerState& state) {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  state = RicohCameraPowerState::Unknown;
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  std::vector<uint8_t> value;
  String err;
  if (!readHandleWithResponse(client, RICOH_BLE_GR4_POWER_STATE_HANDLE, value, err)) {
    _lastError = String("BLE power read failed: ") + err;
    return false;
  }
  if (value.empty()) {
    _lastError = "BLE power read empty";
    return false;
  }

  const uint8_t code = value[0];
  Serial.printf("BLE: power handle=0x%04X read value=0x%02X\n",
                RICOH_BLE_GR4_POWER_STATE_HANDLE,
                code);
  if (code == RICOH_BLE_GR4_POWER_STATE_ON_VALUE) {
    state = RicohCameraPowerState::On;
    _lastError = "";
    return true;
  }
  if (code == RICOH_BLE_GR4_POWER_STATE_OFF_VALUE) {
    state = RicohCameraPowerState::OffOrShuttingDown;
    _lastError = "";
    return true;
  }

  _lastError = String("BLE power unknown value=0x") + String(code, HEX);
  return true;
}

bool RicohBleClient::readOperationMode(RicohCameraOperationMode& mode) {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  mode = RicohCameraOperationMode::Unknown;
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  NimBLERemoteService* cameraService = client->getService(NimBLEUUID(RICOH_BLE_CAMERA_SERVICE_UUID));
  if (cameraService == nullptr) {
    _lastError = "BLE camera service unavailable";
    return false;
  }

  NimBLERemoteCharacteristic* operationMode =
      cameraService->getCharacteristic(NimBLEUUID(RICOH_BLE_OPERATION_MODE_UUID));
  if (operationMode == nullptr || !operationMode->canRead()) {
    _lastError = "BLE operation mode unavailable";
    return false;
  }

  NimBLEAttValue value = operationMode->readValue();
  if (value.length() == 0) {
    _lastError = "BLE operation mode read empty";
    return false;
  }

  const uint8_t code = value.data()[0];
  switch (code) {
    case 0x00:
      mode = RicohCameraOperationMode::Capture;
      break;
    case 0x01:
      mode = RicohCameraOperationMode::Playback;
      break;
    case 0x02:
      mode = RicohCameraOperationMode::BleStartup;
      break;
    case 0x03:
      mode = RicohCameraOperationMode::Other;
      break;
    case 0x04:
      mode = RicohCameraOperationMode::PowerOffTransfer;
      break;
    default:
      mode = RicohCameraOperationMode::Unknown;
      break;
  }

  Serial.printf("BLE: operation mode read value=0x%02X state=%s\n", code, ricohOperationModeName(mode));
  _lastError = "";
  return true;
}

bool RicohBleClient::enablePowerStateNotify() {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  const uint8_t payload[] = {0x01, 0x00};
  String err;
  if (!writeHandleWithResponse(client, RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE, payload, sizeof(payload), err)) {
    _lastError = String("BLE power notify enable failed: ") + err;
    return false;
  }

  _lastError = "";
  Serial.printf("BLE: power notify enabled cccd=0x%04X\n", RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE);
  return true;
}

bool RicohBleClient::consumePowerOffNotification() {
  return g_powerOffNotifyPending.exchange(false);
}

bool RicohBleClient::waitForWifiCredentials(RicohBleWifiCredentials& credentials, uint32_t timeoutMs) {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  credentials = RicohBleWifiCredentials{};
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  const uint32_t startMs = millis();
  while (millis() - startMs < timeoutMs) {
    if (!isConnected() || !client->isConnected()) {
      _lastError = "BLE lost while waiting WiFi params";
      return false;
    }

    RicohBleWifiCredentials current = credentials;
    for (const WlanParamHandle& item : kWlanParamHandles) {
      std::vector<uint8_t> value;
      String err;
      if (readHandleWithResponse(client, item.handle, value, err)) {
        mergeCredentialValue(current, item.label, value);
      }
      delay(20);
      yield();
    }

    credentials = current;
    if (credentials.valid) {
      _lastError = "";
      Serial.printf("BLE: Wi-Fi parameters received ssid='%s' bssid='%s' freq=%u channel=%u wait_ms=%lu\n",
                    credentials.ssid.c_str(),
                    credentials.bssid.c_str(),
                    static_cast<unsigned>(credentials.frequencyMhz),
                    static_cast<unsigned>(credentials.channel),
                    static_cast<unsigned long>(millis() - startMs));
      return true;
    }

    delay(RICOH_BLE_WIFI_CREDENTIAL_POLL_MS);
    yield();
  }

  if (credentials.ssid.length() == 0) {
    _lastError = "BLE WiFi params missing SSID";
  } else if (credentials.encryptedPassphrase) {
    _lastError = "BLE WiFi passphrase encrypted/unparsed";
  } else {
    _lastError = "BLE WiFi params missing passphrase";
  }
  return false;
}

bool RicohBleClient::shoot(bool autofocus) {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  // RICOH GR uses a single capture operation instead of a generic
  // half-press/full-press/release characteristic.  Keep this aligned with the
  // furble Ricoh implementation: ShootingFlavor=IMMEDIATE, then
  // OperationRequest={START, AF|NO_AF}.  There is no release write.
  NimBLERemoteService* shootingService = client->getService(NimBLEUUID(RICOH_BLE_SHOOTING_SERVICE_UUID));
  String err;
  NimBLERemoteCharacteristic* shootingFlavor =
      writableCharacteristic(shootingService, RICOH_BLE_SHOOTING_FLAVOR_UUID, "ShootingFlavor", err);
  if (shootingFlavor == nullptr) {
    _lastError = err;
    return false;
  }

  NimBLERemoteCharacteristic* operationRequest =
      writableCharacteristic(shootingService, RICOH_BLE_OPERATION_REQUEST_UUID, "OperationRequest", err);
  if (operationRequest == nullptr) {
    _lastError = err;
    return false;
  }

  const uint8_t flavorPayload[] = {RICOH_SHOOTING_FLAVOR_IMMEDIATE};
  if (!writeCharacteristicValue(shootingFlavor, flavorPayload, sizeof(flavorPayload), "ShootingFlavor", err)) {
    _lastError = err;
    return false;
  }

  const uint8_t operationParam = autofocus ? RICOH_OPERATION_PARAM_AF : RICOH_OPERATION_PARAM_NO_AF;
  const uint8_t operationPayload[] = {RICOH_OPERATION_START, operationParam};
  if (!writeCharacteristicValue(operationRequest, operationPayload, sizeof(operationPayload), "OperationRequest", err)) {
    _lastError = err;
    return false;
  }

  _lastError = "";
  Serial.printf("BLE: Ricoh shutter OperationRequest START param=%u autofocus=%d\n",
                static_cast<unsigned>(operationParam),
                autofocus ? 1 : 0);
  return true;
}

void RicohBleClient::disconnect() {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (client != nullptr) {
    if (client->isConnected()) {
      client->disconnect();
      const uint32_t startMs = millis();
      while (client->isConnected() && (millis() - startMs) < BLE_DISCONNECT_WAIT_MS) {
        delay(20);
        yield();
      }
    }
    NimBLEDevice::deleteClient(client);
  }
  _client = nullptr;
  _connected = false;
}

int RicohBleClient::consumeDisconnectReason() {
  const int powerOffReason = g_powerOffDisconnectReason.exchange(0);
  if (powerOffReason != 0) {
    g_lastDisconnectReason.exchange(0);
    return powerOffReason;
  }
  return g_lastDisconnectReason.exchange(0);
}

void RicohBleClient::clearDisconnectReason() {
  g_lastDisconnectReason.store(0);
  g_powerOffDisconnectReason.store(0);
  g_powerStateNotifyValue.store(-1);
  g_powerOffNotifyPending.store(false);
}

bool RicohBleClient::deleteAllBonds() {
  begin();
  disconnect();
  const int before = NimBLEDevice::getNumBonds();
  const bool ok = NimBLEDevice::deleteAllBonds();
  const int after = NimBLEDevice::getNumBonds();
  Serial.printf("BLE: delete all bonds before=%d after=%d ok=%d\n", before, after, ok ? 1 : 0);
  if (!ok) {
    _lastError = "NimBLE deleteAllBonds failed";
    return false;
  }
  _lastError = "";
  return true;
}

void RicohBleClient::resetStack(bool clearObjects) {
  Serial.printf("BLE: resetting stack%s\n", clearObjects ? " (clear objects)" : "");
  disconnect();
  NimBLEDevice::deinit(clearObjects);
  _begun = false;
  _lastFailureResourceExhausted = false;
  _lastError = "BLE stack reset";
  delay(BLE_STACK_RESET_DELAY_MS);
  begin();
}

bool RicohBleClient::lastFailureWasResourceExhausted() const {
  return _lastFailureResourceExhausted;
}

String RicohBleClient::statusText() const {
  if (isConnected()) {
    return "BLE_CONNECTED";
  }
  if (_lastError.length() > 0) {
    return _lastError;
  }
  return _begun ? "BLE_READY" : "BLE_IDLE";
}

const String& RicohBleClient::lastError() const {
  return _lastError;
}
