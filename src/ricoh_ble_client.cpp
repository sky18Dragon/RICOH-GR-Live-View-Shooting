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
std::atomic<uint16_t> g_powerStateNotifyHandle{RICOH_BLE_GR4_POWER_STATE_HANDLE};
std::atomic<int> g_pairingGeneration{static_cast<int>(RicohProtocolGeneration::Unknown)};
std::atomic<bool> g_passkeyEntryRequested{false};
// Valid passkeys are <= 999999; the sentinel means "nothing submitted".
constexpr uint32_t kNoSubmittedPasskey = 0xFFFFFFFF;
std::atomic<uint32_t> g_submittedPasskey{kNoSubmittedPasskey};
RicohBleClient::ServiceCallback g_serviceCallback = nullptr;
RicohBleClient::PasskeyPromptCallback g_passkeyPromptCallback = nullptr;

bool pairingGenerationIsGr3() {
  return g_pairingGeneration.load() == static_cast<int>(RicohProtocolGeneration::Gr3Family);
}

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
      event->notify_rx.attr_handle != g_powerStateNotifyHandle.load() ||
      event->notify_rx.om == nullptr || OS_MBUF_PKTLEN(event->notify_rx.om) < 1) {
    return 0;
  }

  uint8_t value = 0xFF;
  if (os_mbuf_copydata(event->notify_rx.om, 0, sizeof(value), &value) != 0) {
    return 0;
  }

  g_powerStateNotifyValue.store(value);
  Serial.printf("BLE: power notify handle=0x%04X value=0x%02X\n",
                g_powerStateNotifyHandle.load(),
                value);
  // 0x02 is a GR III shutdown/sleep report; its meaning on GR IV is
  // unverified, so never let it tear down a GR IV session.
  if (value == RICOH_BLE_GR4_POWER_STATE_OFF_VALUE ||
      (value == 0x02 && pairingGenerationIsGr3())) {
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
         device->isAdvertisingService(NimBLEUUID(RICOH_BLE_CONTROL_SERVICE_UUID)) ||
         device->isAdvertisingService(NimBLEUUID(RICOH_BLE_GR3_WLAN_SERVICE_UUID));
}

}  // namespace

// Shared with main.cpp (declared in ricoh_ble_client.h) so candidate gating
// and scan scoring cannot drift apart on what counts as a Ricoh identity.
bool hasRicohIdentitySignal(const RicohBleDeviceInfo& info) {
  return info.name.length() > 0 ||
         info.hasInfoService ||
         info.hasCameraService ||
         info.hasShootingService ||
         info.hasControlService ||
         info.hasGr3WlanService;
}

namespace {

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
  info.hasGr3WlanService = device->isAdvertisingService(NimBLEUUID(RICOH_BLE_GR3_WLAN_SERVICE_UUID));
  return info;
}

// A stored-identity match must dominate every sum the heuristics below can
// produce (services + name + RSSI, ~1200 max), otherwise a louder foreign
// camera outranks the user's own camera and starves its reconnect.
constexpr int kPreferredMatchScore = 2500;

int candidateScore(const RicohBleDeviceInfo& info, const String& preferredAddress, const String& preferredName) {
  int score = 0;
  if (info.connectable) {
    score += 2000;
  }
  if (nameMatchesPreferred(info.name, preferredName) ||
      addressMatches(info.address, preferredAddress)) {
    score += kPreferredMatchScore;
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
  if (info.hasGr3WlanService) {
    score += 350;
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
      : _preferredAddress(preferredAddress),
        _preferredName(preferredName),
        _acceptFirstRicoh(preferredAddress.length() == 0 && preferredName.length() == 0) {}

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
        (_acceptFirstRicoh ||
         addressMatches(info.address, _preferredAddress) ||
         nameMatchesPreferred(info.name, _preferredName))) {
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
  bool _acceptFirstRicoh = false;
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
    // A passkey-input request implies the keyboard IO capability, which is
    // only presented for GR III links. If the generation is still Unknown
    // (e.g. camera-initiated pairing raced detection), prompting is the safe
    // default: injecting 123456 into a GR III exchange fails pairing.
    if (g_pairingGeneration.load() == static_cast<int>(RicohProtocolGeneration::Gr4Family)) {
      NimBLEDevice::injectPassKey(connInfo, 123456);
      return;
    }
    g_passkeyEntryRequested.store(true);
  }

  uint32_t onPassKeyDisplay(NimBLEConnInfo&) override {
    return 123456;
  }

  void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pin) override {
    // GR III security rests on passkey entry. Auto-confirming a numeric
    // comparison would mint an "authenticated" key the user never verified,
    // so reject it rather than fake the MITM protection we later assert.
    if (pairingGenerationIsGr3()) {
      Serial.printf("BLE security: rejecting numeric-comparison pairing %06lu for GR III (passkey entry required)\n",
                    static_cast<unsigned long>(pin));
      NimBLEDevice::injectConfirmPasskey(connInfo, false);
      return;
    }
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

// GR IV pairing was hardware-tuned with DisplayYesNo; presenting a keyboard
// capability changes the SMP association model the camera negotiates. Only a
// GR III link may advertise keyboard input (its passkey-entry pairing).
void applySecurityIoCapForGeneration(RicohProtocolGeneration generation) {
  NimBLEDevice::setSecurityIOCap(generation == RicohProtocolGeneration::Gr3Family
                                     ? BLE_HS_IO_KEYBOARD_DISPLAY
                                     : BLE_HS_IO_DISPLAY_YESNO);
}

void configureRicohSecurity() {
  NimBLEDevice::setSecurityAuth(true, true, true);
  applySecurityIoCapForGeneration(RicohProtocolGeneration::Unknown);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT);
}

// Shared between the app task and the NimBLE host task. Ownership handoff:
// both the finishing callback and the abandoning waiter call release*Context
// exactly once; the exchange guarantees only the second caller deletes. If a
// timed-out procedure never invokes its callback the context leaks. That is rare,
// small, and preferable to a use-after-free.
struct WriteContext {
  std::atomic<bool> done{false};
  std::atomic<int> status{BLE_HS_EUNKNOWN};
  std::atomic<bool> released{false};
};

void releaseWriteContext(WriteContext* ctx) {
  if (ctx != nullptr && ctx->released.exchange(true)) {
    delete ctx;
  }
}

int handleWriteCallback(uint16_t, const ble_gatt_error* error, ble_gatt_attr*, void* arg) {
  WriteContext* ctx = static_cast<WriteContext*>(arg);
  if (ctx == nullptr) {
    return 0;
  }
  ctx->status.store(error != nullptr ? error->status : BLE_HS_EUNKNOWN);
  ctx->done.store(true);
  releaseWriteContext(ctx);
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
  while (!ctx->done.load() && (millis() - startMs) < HANDLE_WRITE_TIMEOUT_MS) {
    delay(10);
    yield();
  }
  if (!ctx->done.load()) {
    releaseWriteContext(ctx);
    if (client->isConnected()) {
      client->disconnect();
    }
    errorOut = "NimBLE write timeout";
    return false;
  }

  const int status = ctx->status.load();
  releaseWriteContext(ctx);
  if (status != 0) {
    errorOut = String("NimBLE write failed status=") + String(static_cast<int>(status));
    return false;
  }
  errorOut = "";
  return true;
}

struct ReadContext {
  std::atomic<bool> done{false};
  std::atomic<int> status{BLE_HS_EUNKNOWN};
  std::atomic<bool> released{false};
  bool longRead = false;
  std::vector<uint8_t> value;
};

void releaseReadContext(ReadContext* ctx) {
  if (ctx != nullptr && ctx->released.exchange(true)) {
    delete ctx;
  }
}

void finishReadContext(ReadContext* ctx, int status) {
  if (ctx == nullptr) {
    return;
  }
  ctx->status.store(status);
  ctx->done.store(true);
  releaseReadContext(ctx);
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
  while (!ctx->done.load() && (millis() - startMs) < HANDLE_READ_TIMEOUT_MS) {
    delay(10);
    yield();
  }
  if (!ctx->done.load()) {
    releaseReadContext(ctx);
    if (client->isConnected()) {
      client->disconnect();
    }
    errorOut = "NimBLE read timeout";
    return false;
  }

  const int status = ctx->status.load();
  const bool ok = (status == 0 || status == BLE_HS_EDONE);
  if (ok) {
    value = ctx->value;
    errorOut = "";
  } else {
    errorOut = String("NimBLE read failed status=") + String(static_cast<int>(status));
  }
  releaseReadContext(ctx);
  return ok;
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

NimBLERemoteCharacteristic* findCharacteristic(NimBLEClient* client,
                                               const char* serviceUuid,
                                               const char* characteristicUuid) {
  if (client == nullptr || !client->isConnected()) {
    return nullptr;
  }
  NimBLERemoteService* service = client->getService(NimBLEUUID(serviceUuid));
  if (service == nullptr) {
    return nullptr;
  }
  return service->getCharacteristic(NimBLEUUID(characteristicUuid));
}

ProtocolDetectionEvidence collectProtocolEvidence(NimBLEClient* client,
                                                  bool advertisedGr4Control,
                                                  bool advertisedGr3Wlan,
                                                  bool probeGr4ReadHandle) {
  ProtocolDetectionEvidence evidence;
  evidence.hasGr4ControlService = advertisedGr4Control;
  evidence.hasGr3WlanService = advertisedGr3Wlan;
  if (client == nullptr || !client->isConnected()) {
    return evidence;
  }

  // getService()/getCharacteristic() perform targeted discovery on a cache
  // miss, so no full GATT walk is needed here.
  NimBLERemoteService* gr3Wlan =
      client->getService(NimBLEUUID(RICOH_BLE_GR3_WLAN_SERVICE_UUID));
  evidence.hasGr3WlanService = evidence.hasGr3WlanService || gr3Wlan != nullptr;
  evidence.hasGr3NetworkTypeCharacteristic =
      gr3Wlan != nullptr &&
      gr3Wlan->getCharacteristic(NimBLEUUID(RICOH_BLE_GR3_WLAN_NETWORK_TYPE_UUID)) != nullptr;

  // Never probe the GR IV fixed handle once anything GR III shaped is in
  // sight: on a GR III that handle is an arbitrary attribute, and a lucky
  // read would misclassify the camera as GR IV and unlock fixed-handle
  // writes against it.
  if (probeGr4ReadHandle && !evidence.hasGr3WlanService &&
      !evidence.hasGr3NetworkTypeCharacteristic) {
    std::vector<uint8_t> value;
    String error;
    evidence.gr4PowerHandleReadSucceeded =
        readHandleWithResponse(client, RICOH_BLE_GR4_POWER_STATE_HANDLE, value, error) &&
        !value.empty();
  }
  return evidence;
}

int finishGr3PairingRead(uint16_t, const ble_gatt_error*, ble_gatt_attr*, void*) {
  return 0;
}

bool armGr3PairingWithProtectedRead(NimBLEClient* client) {
  NimBLERemoteCharacteristic* ssid =
      findCharacteristic(client, RICOH_BLE_GR3_WLAN_SERVICE_UUID, RICOH_BLE_GR3_WLAN_SSID_UUID);
  if (ssid == nullptr) {
    return false;
  }
  const int rc =
      ble_gattc_read(client->getConnHandle(), ssid->getHandle(), finishGr3PairingRead, nullptr);
  if (rc != 0) {
    Serial.printf("BLE pairing: protected SSID read start failed rc=%d\n", rc);
    return false;
  }
  Serial.println("BLE pairing: protected GR III read requested");
  delay(300);
  yield();
  return true;
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
                              String& errorOut,
                              bool response = true) {
  if (characteristic == nullptr || payload == nullptr || length == 0) {
    errorOut = String("BLE ") + label + " invalid write";
    return false;
  }

  if (!characteristic->writeValue(payload, length, response)) {
    errorOut = String("BLE ") + label + " write failed";
    return false;
  }

  errorOut = "";
  return true;
}

bool waitForEncryptedConnection(NimBLEClient* client,
                                uint32_t timeoutMs,
                                bool requireAuthenticatedBond,
                                String& errorOut) {
  if (client == nullptr || !client->isConnected()) {
    errorOut = "BLE not connected";
    return false;
  }

  const uint32_t startMs = millis();
  uint32_t effectiveTimeoutMs = timeoutMs;
  uint32_t passkey = 0;
  uint8_t passkeyDigits = 0;
  bool passkeyPromptActive = false;
  while (client->isConnected() && (millis() - startMs) < effectiveTimeoutMs) {
    if (g_serviceCallback != nullptr && g_serviceCallback()) {
      errorOut = "BLE operation aborted";
      return false;
    }
    NimBLEConnInfo info = client->getConnInfo();
    if (info.isEncrypted() &&
        (!requireAuthenticatedBond || (info.isAuthenticated() && info.isBonded()))) {
      g_passkeyEntryRequested.store(false);
      errorOut = "";
      return true;
    }

    if (g_passkeyEntryRequested.load()) {
      if (!passkeyPromptActive) {
        passkeyPromptActive = true;
        effectiveTimeoutMs = std::max(effectiveTimeoutMs, RICOH_BLE_GR3_PASSKEY_ENTRY_WAIT_MS);
        // Bytes queued before the request (line endings, stray keys) are not
        // part of the code; each new request starts from a clean slate.
        while (Serial.available() > 0) {
          (void)Serial.read();
        }
        passkey = 0;
        passkeyDigits = 0;
        if (g_passkeyPromptCallback != nullptr) {
          g_passkeyPromptCallback();
        }
        Serial.println("BLE security: type the camera's six-digit code in the serial monitor");
      }
      while (Serial.available() > 0 && passkeyDigits < 6) {
        const char c = static_cast<char>(Serial.read());
        if (c < '0' || c > '9') {
          continue;
        }
        passkey = passkey * 10 + static_cast<uint32_t>(c - '0');
        ++passkeyDigits;
      }
      const uint32_t buttonPasskey = g_submittedPasskey.exchange(kNoSubmittedPasskey);
      const bool haveCode = passkeyDigits == 6 || buttonPasskey != kNoSubmittedPasskey;
      if (haveCode) {
        const uint32_t code = buttonPasskey != kNoSubmittedPasskey ? buttonPasskey : passkey;
        const bool injected = NimBLEDevice::injectPassKey(client->getConnInfo(), code);
        g_passkeyEntryRequested.store(false);
        passkeyPromptActive = false;
        passkey = 0;
        passkeyDigits = 0;
        if (!injected) {
          errorOut = "BLE passkey submission failed (pairing likely timed out)";
          return false;
        }
        Serial.println("BLE security: GR III passkey submitted");
      }
    }
    delay(50);
    yield();
  }

  g_passkeyEntryRequested.store(false);
  if (client->isConnected() && requireAuthenticatedBond && client->getConnInfo().isEncrypted()) {
    errorOut = "BLE security completed without authenticated bond";
  } else {
    errorOut = client->isConnected() ? "BLE security timeout" : "BLE lost during security";
  }
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

void RicohBleClient::setPasskeyPromptCallback(PasskeyPromptCallback callback) {
  g_passkeyPromptCallback = callback;
}

bool RicohBleClient::passkeyEntryPending() {
  return g_passkeyEntryRequested.load();
}

void RicohBleClient::submitPasskey(uint32_t passkey) {
  if (passkey <= 999999) {
    g_submittedPasskey.store(passkey);
  }
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

  // Seed the pairing generation from the advertisement before the link comes
  // up: a camera-initiated Security Request can arrive ahead of any GATT
  // detection, and the passkey and IO-capability paths must already know what
  // they are talking to.
  RicohProtocolGeneration provisionalGeneration =
      info.hasGr3WlanService ? RicohProtocolGeneration::Gr3Family
      : (info.hasControlService ? RicohProtocolGeneration::Gr4Family
                                : RicohProtocolGeneration::Unknown);
  g_pairingGeneration.store(static_cast<int>(provisionalGeneration));
  applySecurityIoCapForGeneration(provisionalGeneration);

  _client = client;
  client->setClientCallbacks(&g_callbacks, false);
  client->setConnectTimeout(options.timeoutMs);
  client->setConnectRetries(1);
  client->setConnectionParams(RICOH_BLE_SHUTTER_CONN_INTERVAL_MIN,
                              RICOH_BLE_SHUTTER_CONN_INTERVAL_MAX,
                              RICOH_BLE_SHUTTER_CONN_LATENCY,
                              RICOH_BLE_SHUTTER_SUPERVISION_TIMEOUT);

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
  const bool peerBonded = NimBLEDevice::isBonded(peer);
  // Fresh pairings that the advertisement could not classify get one targeted
  // GATT probe before security so the right IO capability is presented.
  // Bonded reconnects skip all of this: encryption restores from the stored
  // LTK and every extra pre-security exchange just delays it.
  if (!alreadyEncrypted && !peerBonded) {
    if (provisionalGeneration == RicohProtocolGeneration::Unknown) {
      const ProtocolDetectionEvidence preSecurityEvidence =
          collectProtocolEvidence(client, info.hasControlService, info.hasGr3WlanService, false);
      provisionalGeneration = detectRicohProtocol(preSecurityEvidence);
      g_pairingGeneration.store(static_cast<int>(provisionalGeneration));
      applySecurityIoCapForGeneration(provisionalGeneration);
    }
    if (provisionalGeneration == RicohProtocolGeneration::Gr3Family) {
      (void)armGr3PairingWithProtectedRead(client);
    }
  }

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
  const bool requireAuthenticatedBond =
      provisionalGeneration == RicohProtocolGeneration::Gr3Family;
  if (!waitForEncryptedConnection(client,
                                  securityWaitMs,
                                  requireAuthenticatedBond,
                                  securityWaitError)) {
    _lastFailureResourceExhausted = false;
    _lastError = securityWaitError;
    Serial.printf("BLE: security wait failed after %lums total_ms=%lums: %s\n",
                  static_cast<unsigned long>(millis() - securityStartMs),
                  static_cast<unsigned long>(millis() - connectStartMs),
                  securityWaitError.c_str());
    disconnect();
    return false;
  }

  const ProtocolDetectionEvidence evidence =
      collectProtocolEvidence(client, info.hasControlService, info.hasGr3WlanService, true);
  _protocolGeneration = detectRicohProtocol(evidence);
  g_pairingGeneration.store(static_cast<int>(_protocolGeneration));
  Serial.printf("BLE: protocol profile=%s\n",
                ricohProtocolGenerationName(_protocolGeneration));
  if (_protocolGeneration == RicohProtocolGeneration::Unknown) {
    _lastError = "RICOH BLE protocol not recognized; writes blocked";
    disconnect();
    return false;
  }

  const NimBLEConnInfo securityInfo = client->getConnInfo();
  if (cameraProtocolProfile(_protocolGeneration).requiresAuthenticatedLink &&
      (!securityInfo.isEncrypted() || !securityInfo.isAuthenticated() || !securityInfo.isBonded())) {
    _lastError = "GR III requires an authenticated bonded BLE link";
    disconnect();
    return false;
  }

  const bool connUpdateStarted =
      client->updateConnParams(RICOH_BLE_SHUTTER_CONN_INTERVAL_MIN,
                               RICOH_BLE_SHUTTER_CONN_INTERVAL_MAX,
                               RICOH_BLE_SHUTTER_CONN_LATENCY,
                               RICOH_BLE_SHUTTER_SUPERVISION_TIMEOUT);
  Serial.printf("BLE: shutter latency params requested interval=%u-%u latency=%u started=%d\n",
                static_cast<unsigned>(RICOH_BLE_SHUTTER_CONN_INTERVAL_MIN),
                static_cast<unsigned>(RICOH_BLE_SHUTTER_CONN_INTERVAL_MAX),
                static_cast<unsigned>(RICOH_BLE_SHUTTER_CONN_LATENCY),
                connUpdateStarted ? 1 : 0);

  const uint32_t shutterPrepareStartMs = millis();
  if (!prepareShutter()) {
    Serial.printf("BLE: shutter warm-up deferred after %lums: %s\n",
                  static_cast<unsigned long>(millis() - shutterPrepareStartMs),
                  _lastError.c_str());
    // A failed warm-up must not reject an otherwise healthy camera connection.
    // shoot() retries discovery lazily on the first button press.
    resetShutterCache();
  } else {
    Serial.printf("BLE: shutter warm-up ready in %lums\n",
                  static_cast<unsigned long>(millis() - shutterPrepareStartMs));
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

void RicohBleClient::resetShutterCache() {
  _shutterPrepared = false;
  _shootingFlavor = nullptr;
  _operationRequest = nullptr;
}

bool RicohBleClient::prepareShutter() {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }
  if (_shutterPrepared && _shootingFlavor != nullptr && _operationRequest != nullptr) {
    return true;
  }

  resetShutterCache();
  NimBLERemoteService* shootingService =
      client->getService(NimBLEUUID(RICOH_BLE_SHOOTING_SERVICE_UUID));
  String err;
  NimBLERemoteCharacteristic* shootingFlavor =
      writableCharacteristic(shootingService,
                             RICOH_BLE_SHOOTING_FLAVOR_UUID,
                             "ShootingFlavor",
                             err);
  if (shootingFlavor == nullptr) {
    _lastError = err;
    return false;
  }

  NimBLERemoteCharacteristic* operationRequest =
      writableCharacteristic(shootingService,
                             RICOH_BLE_OPERATION_REQUEST_UUID,
                             "OperationRequest",
                             err);
  if (operationRequest == nullptr) {
    _lastError = err;
    return false;
  }

  const uint8_t flavorPayload[] = {RICOH_SHOOTING_FLAVOR_IMMEDIATE};
  const bool flavorNeedsResponse = !shootingFlavor->canWriteNoResponse();
  if (!writeCharacteristicValue(shootingFlavor,
                                flavorPayload,
                                sizeof(flavorPayload),
                                "ShootingFlavor",
                                err,
                                flavorNeedsResponse)) {
    _lastError = err;
    return false;
  }

  _shootingFlavor = shootingFlavor;
  _operationRequest = operationRequest;
  _shutterPrepared = true;
  _lastError = "";
  return true;
}

bool RicohBleClient::openWifi() {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  switch (cameraProtocolProfile(_protocolGeneration).wifiActivationMethod) {
    case WifiActivationMethod::BleNetworkTypeUuid: {
      const NimBLEConnInfo security = client->getConnInfo();
      if (!security.isEncrypted() || !security.isAuthenticated()) {
        _lastError = "GR III Wi-Fi activation requires authenticated encryption";
        return false;
      }
      // The camera only honors the network-type write in Capture mode; read
      // it here so activation cannot depend on what a caller did earlier.
      RicohCameraOperationMode mode = RicohCameraOperationMode::Unknown;
      if (!readOperationMode(mode)) {
        _lastError = String("GR III operation mode read failed: ") + _lastError;
        return false;
      }
      if (mode != RicohCameraOperationMode::Capture) {
        _lastError = String("GR III Wi-Fi requires Capture mode, camera is ") +
                     ricohOperationModeName(mode);
        return false;
      }
      NimBLERemoteCharacteristic* networkType =
          findCharacteristic(client,
                             RICOH_BLE_GR3_WLAN_SERVICE_UUID,
                             RICOH_BLE_GR3_WLAN_NETWORK_TYPE_UUID);
      if (networkType == nullptr || !networkType->canWrite()) {
        _lastError = "GR III Network Type characteristic unavailable";
        return false;
      }
      const uint8_t apMode[] = {0x01};
      String error;
      if (!writeCharacteristicValue(networkType,
                                    apMode,
                                    sizeof(apMode),
                                    "NetworkType",
                                    error)) {
        _lastError = error;
        return false;
      }
      _lastError = "";
      Serial.println("BLE: Wi-Fi open requested method=GR3_NETWORK_TYPE_UUID");
      return true;
    }

    case WifiActivationMethod::BleFixedHandle: {
      const uint8_t payload[] = {RICOH_BLE_GR4_WLAN_ON_VALUE};
      String error;
      if (!writeHandleWithResponse(client,
                                   RICOH_BLE_GR4_WLAN_POWER_HANDLE,
                                   payload,
                                   sizeof(payload),
                                   error)) {
        _lastError = error;
        return false;
      }
      _lastError = "";
      Serial.println("BLE: Wi-Fi open requested method=GR4_FIXED_HANDLE");
      return true;
    }

    case WifiActivationMethod::Unsupported:
      break;
  }

  _lastError = "Wi-Fi activation blocked for unknown BLE protocol";
  return false;
}

bool RicohBleClient::readPowerState(RicohCameraPowerState& state) {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  state = RicohCameraPowerState::Unknown;
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  std::vector<uint8_t> value;
  if (_protocolGeneration == RicohProtocolGeneration::Gr3Family) {
    NimBLERemoteCharacteristic* power =
        findCharacteristic(client, RICOH_BLE_CAMERA_SERVICE_UUID, RICOH_BLE_CAMERA_POWER_UUID);
    if (power == nullptr || !power->canRead()) {
      _lastError = "GR III Camera Power characteristic unavailable";
      return false;
    }
    const NimBLEAttValue readValue = power->readValue();
    if (readValue.length() > 0) {
      value.assign(readValue.data(), readValue.data() + readValue.length());
    }
  } else if (_protocolGeneration == RicohProtocolGeneration::Gr4Family) {
    String error;
    if (!readHandleWithResponse(client,
                                RICOH_BLE_GR4_POWER_STATE_HANDLE,
                                value,
                                error)) {
      _lastError = String("BLE power read failed: ") + error;
      return false;
    }
  } else {
    _lastError = "Power read blocked for unknown BLE protocol";
    return false;
  }
  if (value.empty()) {
    _lastError = "BLE power read empty";
    return false;
  }

  const uint8_t code = value[0];
  Serial.printf("BLE: power profile=%s value=0x%02X\n",
                ricohProtocolGenerationName(_protocolGeneration),
                code);
  if (code == RICOH_BLE_GR4_POWER_STATE_ON_VALUE) {
    state = RicohCameraPowerState::On;
    _lastError = "";
    return true;
  }
  if (code == RICOH_BLE_GR4_POWER_STATE_OFF_VALUE ||
      (_protocolGeneration == RicohProtocolGeneration::Gr3Family && code == 0x02)) {
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

  if (_protocolGeneration == RicohProtocolGeneration::Gr3Family) {
    NimBLERemoteCharacteristic* power =
        findCharacteristic(client, RICOH_BLE_CAMERA_SERVICE_UUID, RICOH_BLE_CAMERA_POWER_UUID);
    if (power != nullptr && power->canNotify()) {
      // Notifications are handled centrally by ricohGapEventHandler; publish
      // the handle before subscribing so the first notification cannot race
      // past the filter.
      g_powerStateNotifyHandle.store(power->getHandle());
      if (power->subscribe(true)) {
        _lastError = "";
        Serial.println("BLE: power notify enabled method=GR3_UUID_DESCRIPTOR");
        return true;
      }
      g_powerStateNotifyHandle.store(RICOH_BLE_GR4_POWER_STATE_HANDLE);
    }
    _lastError = "GR III power notification unavailable";
    return false;
  }

  if (_protocolGeneration == RicohProtocolGeneration::Gr4Family) {
    const uint8_t payload[] = {0x01, 0x00};
    String error;
    g_powerStateNotifyHandle.store(RICOH_BLE_GR4_POWER_STATE_HANDLE);
    if (!writeHandleWithResponse(client,
                                 RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE,
                                 payload,
                                 sizeof(payload),
                                 error)) {
      _lastError = String("BLE power notify enable failed: ") + error;
      return false;
    }
    _lastError = "";
    Serial.printf("BLE: power notify enabled cccd=0x%04X\n",
                  RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE);
    return true;
  }

  _lastError = "Power notification blocked for unknown BLE protocol";
  return false;
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
    const WifiCredentialMethod credentialMethod =
        cameraProtocolProfile(_protocolGeneration).wifiCredentialMethod;
    if (credentialMethod == WifiCredentialMethod::BleUuidCharacteristics) {
      struct Gr3CredentialCharacteristic {
        const char* uuid;
        const char* label;
      };
      const Gr3CredentialCharacteristic parameters[] = {
          {RICOH_BLE_GR3_WLAN_SSID_UUID, "ssid"},
          {RICOH_BLE_GR3_WLAN_PASSPHRASE_UUID, "passphrase"},
          {RICOH_BLE_GR3_WLAN_CHANNEL_UUID, "channel"},
      };

      for (const Gr3CredentialCharacteristic& parameter : parameters) {
        NimBLERemoteCharacteristic* characteristic =
            findCharacteristic(client, RICOH_BLE_GR3_WLAN_SERVICE_UUID, parameter.uuid);
        if (characteristic == nullptr || !characteristic->canRead()) {
          continue;
        }
        const NimBLEAttValue value = characteristic->readValue();
        if (value.length() == 0) {
          continue;
        }
        const std::vector<uint8_t> bytes(value.data(), value.data() + value.length());
        // These characteristics are authoritative: the value IS the field.
        // Applying the GR IV key/value or SSID-shape heuristics here would
        // reject user-renamed SSIDs the camera just handed us.
        if (strcmp(parameter.label, "channel") == 0) {
          current.channel = parseGr3WifiChannel(bytes.data(), bytes.size());
          current.frequencyMhz = frequencyMhzForChannel(current.channel);
        } else if (strcmp(parameter.label, "ssid") == 0) {
          const String text = printableText(bytes);
          if (text.length() > 0 && text.length() <= 32) {
            current.ssid = text;
          }
        } else {
          const String text = printableText(bytes);
          if (text.length() > 0) {
            current.passphrase = text;
            current.encryptedPassphrase = false;
          } else {
            current.encryptedPassphrase = true;
          }
        }
        delay(20);
        yield();
      }
      current.valid = validGr3WifiCredentials(current.ssid.c_str(),
                                              current.passphrase.c_str(),
                                              current.channel);
    } else if (credentialMethod == WifiCredentialMethod::BleFixedHandles) {
      for (const WlanParamHandle& item : kWlanParamHandles) {
        std::vector<uint8_t> value;
        String error;
        if (readHandleWithResponse(client, item.handle, value, error)) {
          mergeCredentialValue(current, item.label, value);
        }
        delay(20);
        yield();
      }
    } else {
      _lastError = "Wi-Fi credential read blocked for unknown BLE protocol";
      return false;
    }

    credentials = current;
    if (credentials.valid) {
      _lastError = "";
      Serial.printf("BLE: Wi-Fi parameters received profile=%s ssid_present=%d passphrase_present=%d bssid_present=%d freq=%u channel=%u wait_ms=%lu\n",
                    ricohProtocolGenerationName(_protocolGeneration),
                    credentials.ssid.length() > 0 ? 1 : 0,
                    credentials.passphrase.length() > 0 ? 1 : 0,
                    credentials.bssid.length() > 0 ? 1 : 0,
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
  } else if (credentials.passphrase.length() == 0) {
    _lastError = "BLE WiFi params missing passphrase";
  } else if (_protocolGeneration == RicohProtocolGeneration::Gr3Family &&
             !validGr3WifiChannel(credentials.channel)) {
    _lastError = String("BLE WiFi channel invalid: ") + String(credentials.channel);
  } else {
    _lastError = "BLE WiFi params incomplete";
  }
  return false;
}

bool RicohBleClient::shoot(bool autofocus) {
  NimBLEClient* client = static_cast<NimBLEClient*>(_client);
  if (!isConnected() || client == nullptr) {
    _lastError = "BLE not connected";
    return false;
  }

  const uint32_t startedAt = millis();
  if (!prepareShutter()) {
    return false;
  }

  String err;
  NimBLERemoteCharacteristic* operationRequest =
      static_cast<NimBLERemoteCharacteristic*>(_operationRequest);
  const uint8_t operationParam = autofocus ? RICOH_OPERATION_PARAM_AF : RICOH_OPERATION_PARAM_NO_AF;
  const uint8_t operationPayload[] = {RICOH_OPERATION_START, operationParam};
  const bool needsResponse = !operationRequest->canWriteNoResponse();
  if (!writeCharacteristicValue(operationRequest,
                                operationPayload,
                                sizeof(operationPayload),
                                "OperationRequest",
                                err,
                                needsResponse)) {
    resetShutterCache();
    _lastError = err;
    return false;
  }

  _lastError = "";
  Serial.printf("BLE: Ricoh shutter dispatched in %lums response=%d param=%u autofocus=%d\n",
                static_cast<unsigned long>(millis() - startedAt),
                needsResponse ? 1 : 0,
                static_cast<unsigned>(operationParam),
                autofocus ? 1 : 0);
  return true;
}

void RicohBleClient::disconnect() {
  resetShutterCache();
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
  _protocolGeneration = RicohProtocolGeneration::Unknown;
  g_pairingGeneration.store(static_cast<int>(RicohProtocolGeneration::Unknown));
  g_passkeyEntryRequested.store(false);
  g_submittedPasskey.store(kNoSubmittedPasskey);
  g_powerStateNotifyHandle.store(RICOH_BLE_GR4_POWER_STATE_HANDLE);
  applySecurityIoCapForGeneration(RicohProtocolGeneration::Unknown);
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
  g_passkeyEntryRequested.store(false);
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

const CameraProtocolProfile& RicohBleClient::protocolProfile() const {
  return cameraProtocolProfile(_protocolGeneration);
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
