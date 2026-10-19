#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "brands/ricoh/common/RicohBleUuids.h"
#include "brands/ricoh/gr3/Gr3BleConfig.h"
#include "brands/ricoh/gr3/Gr3BleProtocol.h"
#include "brands/ricoh/gr4/Gr4BleConfig.h"
#include "brands/ricoh/gr4/Gr4LegacyBleProtocol.h"
#include "brands/ricoh/gr4/Gr4WifiCredentialParser.h"

namespace {

std::string hex(const uint8_t* data, size_t length) { return ricohHexBytes(data, length); }

// Records every GATT primitive a model issues, as "op target [bytes]".
class FakeGattSession final : public RicohGattSession {
public:
  std::vector<std::string> calls;
  std::string out;  // print/println output, println as "\r\n" like Serial

  RicohLinkSecurity link{true, true};
  bool handleWriteOk = true;
  std::string handleWriteError = "ATT write rejected";
  std::map<uint16_t, std::vector<uint8_t>> handleValues;
  std::map<std::string, RicohGattRead> readResults;
  std::map<std::string, std::vector<uint8_t>> characteristicValues;
  std::map<std::string, RicohGattWrite> writeResults;
  std::string rejectedError = "BLE protected read requires authenticated encryption";
  int failedRc = 0;
  bool subscribeOk = true;
  RicohGattStart startResult = RicohGattStart::Started;
  int startRc = 0;
  bool probeFound = true;
  int probeRc = 0;
  uint16_t routedHandle = 0xFFFF;

  bool writeHandle(uint16_t handle, const uint8_t* data, size_t length, std::string& error) override {
    calls.push_back("writeHandle " + handleText(handle) + " [" + hex(data, length) + "]");
    if (!handleWriteOk) {
      error = handleWriteError;
      return false;
    }
    error.clear();
    return true;
  }

  bool readHandle(uint16_t handle, std::vector<uint8_t>& value, std::string& error) override {
    calls.push_back("readHandle " + handleText(handle));
    auto it = handleValues.find(handle);
    if (it == handleValues.end()) {
      error = "ATT read timeout";
      return false;
    }
    value = it->second;
    return true;
  }

  RicohGattRead readCharacteristic(const char* service,
                                   const char* uuid,
                                   const char* label,
                                   std::vector<uint8_t>& value,
                                   std::string& error,
                                   int& rc) override {
    calls.push_back(std::string("read ") + uuid + " " + label + " @" + service);
    const RicohGattRead result = readResults.count(uuid) ? readResults[uuid] : RicohGattRead::Unavailable;
    if (result == RicohGattRead::Ok) value = characteristicValues[uuid];
    if (result == RicohGattRead::Rejected) error = rejectedError;
    if (result == RicohGattRead::Failed) rc = failedRc;
    return result;
  }

  bool readCharacteristicRaw(const char* service, const char* uuid, std::vector<uint8_t>& value) override {
    calls.push_back(std::string("readRaw ") + uuid + " @" + service);
    auto it = characteristicValues.find(uuid);
    if (it == characteristicValues.end()) return false;
    value = it->second;
    return true;
  }

  RicohGattWrite writeCharacteristic(const char* service,
                                     const char* uuid,
                                     const uint8_t* data,
                                     size_t length,
                                     const char* label,
                                     std::string& error) override {
    calls.push_back(std::string("write ") + uuid + " " + label + " [" + hex(data, length) + "] @" + service);
    const RicohGattWrite result = writeResults.count(uuid) ? writeResults[uuid] : RicohGattWrite::Ok;
    if (result == RicohGattWrite::Failed) error = std::string("BLE ") + label + " write failed";
    return result;
  }

  bool probeRead(const char* service, const char* uuid, int& rc) override {
    calls.push_back(std::string("probeRead ") + uuid + " @" + service);
    if (!probeFound) return false;
    rc = probeRc;
    return true;
  }

  RicohGattStart startRead(const char* service, const char* uuid, int& rc) override {
    calls.push_back(std::string("startRead ") + uuid + " @" + service);
    if (startResult == RicohGattStart::Failed) rc = startRc;
    return startResult;
  }

  bool subscribe(const char* service, const char* uuid, RicohNotifyTarget) override {
    calls.push_back(std::string("subscribe ") + uuid + " @" + service);
    return subscribeOk;
  }

  void routeHandleNotify(uint16_t handle, RicohNotifyTarget) override {
    calls.push_back("route " + handleText(handle));
    routedHandle = handle;
  }

  RicohLinkSecurity security() const override { return link; }
  void pause(uint32_t ms) override { calls.push_back("pause " + std::to_string(ms)); }
  void print(const char* text) override { out += text; }
  void println(const char* text) override {
    out += text;
    out += "\r\n";
  }

private:
  static std::string handleText(uint16_t handle) {
    char text[8];
    std::snprintf(text, sizeof(text), "0x%04X", handle);
    return text;
  }
};

std::string join(const std::vector<std::string>& calls) {
  std::string text;
  for (const std::string& call : calls) {
    if (!text.empty()) text += " | ";
    text += call;
  }
  return text;
}

const std::string kGr3Wlan = RICOH_BLE_GR3_WLAN_SERVICE_UUID;
const std::string kNetworkType = RICOH_BLE_GR3_WLAN_NETWORK_TYPE_UUID;
const std::string kCameraService = RICOH_BLE_CAMERA_SERVICE_UUID;
const std::string kCameraPower = RICOH_BLE_CAMERA_POWER_UUID;
const std::string kModeList = RICOH_BLE_OPERATION_MODE_LIST_UUID;
const std::string kMode = RICOH_BLE_OPERATION_MODE_UUID;

// ---- GR IV: fixed handles ----

void testGr4WifiIsTheWlanPowerHandle() {
  Gr4LegacyBleProtocol gr4;
  FakeGattSession gatt;
  std::string error = "stale";
  TEST_ASSERT_TRUE(gr4.openWifi(gatt, RicohWifiGate{}, error));
  TEST_ASSERT_TRUE(error.empty());
  TEST_ASSERT_TRUE(gr4.closeWifi(gatt, error));
  TEST_ASSERT_EQUAL_STRING("writeHandle 0x0135 [01] | writeHandle 0x0135 [00]", join(gatt.calls).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "BLE WLAN activation method=FIXED_HANDLE result=OK\r\n"
      "BLE WLAN deactivation method=FIXED_HANDLE handle=0x0135 value=0x00 result=OK\n",
      gatt.out.c_str());

  FakeGattSession failing;
  failing.handleWriteOk = false;
  TEST_ASSERT_FALSE(gr4.openWifi(failing, RicohWifiGate{}, error));
  TEST_ASSERT_EQUAL_STRING("ATT write rejected", error.c_str());
  TEST_ASSERT_TRUE(failing.out.empty());
}

void testGr4WifiIgnoresTheOperationModeGate() {
  // GR IV gates Wi-Fi in the driver's power check, not on the write itself.
  Gr4LegacyBleProtocol gr4;
  FakeGattSession gatt;
  RicohWifiGate gate;
  gate.operationMode = RicohCameraOperationMode::BleStartup;
  gate.operationModeValid = true;
  gatt.link = RicohLinkSecurity{};
  std::string error;
  TEST_ASSERT_TRUE(gr4.openWifi(gatt, gate, error));
}

void testGr4PowerIsReadAndSubscribedByHandle() {
  Gr4LegacyBleProtocol gr4;
  FakeGattSession gatt;
  gatt.handleValues[RICOH_BLE_GR4_POWER_STATE_HANDLE] = {0x01};
  std::vector<uint8_t> value;
  std::string error;
  TEST_ASSERT_TRUE(gr4.readPowerValue(gatt, value, error));
  TEST_ASSERT_EQUAL_UINT32(1, value.size());
  TEST_ASSERT_EQUAL_UINT8(0x01, value[0]);

  TEST_ASSERT_TRUE(gr4.enablePowerNotify(gatt, error));
  TEST_ASSERT_EQUAL_STRING("readHandle 0x00EB | route 0x00EB | writeHandle 0x00EC [01 00]",
                           join(gatt.calls).c_str());
  TEST_ASSERT_EQUAL_UINT16(RICOH_BLE_GR4_POWER_STATE_HANDLE, gatt.routedHandle);
  TEST_ASSERT_EQUAL_STRING("BLE: power notify enabled cccd=0x00EC\n", gatt.out.c_str());

  FakeGattSession missing;
  TEST_ASSERT_FALSE(gr4.readPowerValue(missing, value, error));
  TEST_ASSERT_EQUAL_STRING("BLE power read failed: ATT read timeout", error.c_str());
}

void testGr4NotifyFailureStopsRoutingTheHandle() {
  Gr4LegacyBleProtocol gr4;
  FakeGattSession gatt;
  gatt.handleWriteOk = false;
  std::string error;
  TEST_ASSERT_FALSE(gr4.enablePowerNotify(gatt, error));
  TEST_ASSERT_EQUAL_STRING("route 0x00EB | writeHandle 0x00EC [01 00] | route 0x0000", join(gatt.calls).c_str());
  TEST_ASSERT_EQUAL_UINT16(0, gatt.routedHandle);
  TEST_ASSERT_EQUAL_STRING("BLE power notify enable failed: ATT write rejected", error.c_str());
}

void testGr4HasNoCaptureModeRequest() {
  Gr4LegacyBleProtocol gr4;
  FakeGattSession gatt;
  std::string error;
  TEST_ASSERT_FALSE(gr4.requestCaptureOperationMode(gatt, error));
  TEST_ASSERT_EQUAL_STRING("BLE capture mode request is GR III family only", error.c_str());
  TEST_ASSERT_TRUE(gatt.calls.empty());
}

// ---- GR III family: UUIDs behind authenticated encryption ----

RicohWifiGate readyGate() {
  RicohWifiGate gate;
  gate.operationMode = RicohCameraOperationMode::Other;  // GR IIIx ready to shoot
  gate.operationModeValid = true;
  return gate;
}

void testGr3WifiWritesNetworkType() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  std::string error;
  TEST_ASSERT_TRUE(gr3.openWifi(gatt, readyGate(), error));
  TEST_ASSERT_TRUE(gr3.closeWifi(gatt, error));
  const std::string expected = "write " + kNetworkType + " NetworkType [01] @" + kGr3Wlan + " | write " +
                               kNetworkType + " NetworkTypeOff [00] @" + kGr3Wlan;
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(gatt.calls).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "BLE WLAN activation method=NETWORK_TYPE_UUID result=OK\r\n"
      "BLE WLAN deactivation method=NETWORK_TYPE_UUID value=0x00 result=OK\r\n",
      gatt.out.c_str());
}

void testGr3WifiNeedsAnAuthenticatedLink() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  gatt.link.authenticated = false;
  std::string error;
  TEST_ASSERT_FALSE(gr3.openWifi(gatt, readyGate(), error));
  TEST_ASSERT_EQUAL_STRING("BLE UUID WiFi activation requires authenticated encryption", error.c_str());
  TEST_ASSERT_FALSE(gr3.closeWifi(gatt, error));
  TEST_ASSERT_EQUAL_STRING("BLE UUID WiFi deactivation requires authenticated encryption", error.c_str());
  TEST_ASSERT_TRUE(gatt.calls.empty());
}

void testGr3WifiHonoursTheOperationModeGate() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  RicohWifiGate gate;
  gate.operationMode = RicohCameraOperationMode::BleStartup;
  gate.operationModeValid = true;
  std::string error;
  TEST_ASSERT_FALSE(gr3.openWifi(gatt, gate, error));
  TEST_ASSERT_EQUAL_STRING("BLE UUID WiFi activation blocked by camera operation mode", error.c_str());
  TEST_ASSERT_TRUE(gatt.calls.empty());

  // An unread mode never authorises a write.
  gate.operationMode = RicohCameraOperationMode::Capture;
  gate.operationModeValid = false;
  TEST_ASSERT_FALSE(gr3.openWifi(gatt, gate, error));
  TEST_ASSERT_TRUE(gatt.calls.empty());

  // The explicit startup-mode probe may write; the camera arbitrates it.
  gate.operationMode = RicohCameraOperationMode::BleStartup;
  gate.operationModeValid = true;
  gate.startupModeProbe = true;
  TEST_ASSERT_TRUE(gr3.openWifi(gatt, gate, error));
  TEST_ASSERT_EQUAL_UINT32(1, gatt.calls.size());
}

void testGr3WifiReportsAMissingCharacteristic() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  gatt.writeResults[kNetworkType] = RicohGattWrite::Unavailable;
  std::string error;
  TEST_ASSERT_FALSE(gr3.openWifi(gatt, readyGate(), error));
  TEST_ASSERT_EQUAL_STRING("BLE Network Type characteristic unavailable", error.c_str());

  gatt.writeResults[kNetworkType] = RicohGattWrite::Failed;
  TEST_ASSERT_FALSE(gr3.closeWifi(gatt, error));
  TEST_ASSERT_EQUAL_STRING("BLE NetworkTypeOff write failed", error.c_str());
  TEST_ASSERT_TRUE(gatt.out.empty());
}

void testGr3PowerReadOutcomes() {
  Gr3BleProtocol gr3;
  std::vector<uint8_t> value;
  std::string error;

  FakeGattSession ok;
  ok.readResults[kCameraPower] = RicohGattRead::Ok;
  ok.characteristicValues[kCameraPower] = {0x02};
  TEST_ASSERT_TRUE(gr3.readPowerValue(ok, value, error));
  TEST_ASSERT_EQUAL_UINT8(0x02, value[0]);
  const std::string expected = "read " + kCameraPower + " CameraPower @" + kCameraService;
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(ok.calls).c_str());

  FakeGattSession missing;
  TEST_ASSERT_FALSE(gr3.readPowerValue(missing, value, error));
  TEST_ASSERT_EQUAL_STRING("BLE Camera Power characteristic unavailable", error.c_str());

  FakeGattSession rejected;
  rejected.readResults[kCameraPower] = RicohGattRead::Rejected;
  TEST_ASSERT_FALSE(gr3.readPowerValue(rejected, value, error));
  TEST_ASSERT_EQUAL_STRING("BLE protected read requires authenticated encryption", error.c_str());

  FakeGattSession failed;
  failed.readResults[kCameraPower] = RicohGattRead::Failed;
  failed.failedRc = 7;
  TEST_ASSERT_FALSE(gr3.readPowerValue(failed, value, error));
  TEST_ASSERT_EQUAL_STRING("BLE power UUID read failed rc=7", error.c_str());
}

void testGr3PowerNotifySubscribesTheUuid() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  std::string error;
  TEST_ASSERT_TRUE(gr3.enablePowerNotify(gatt, error));
  const std::string expected = "subscribe " + kCameraPower + " @" + kCameraService;
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(gatt.calls).c_str());
  TEST_ASSERT_EQUAL_STRING("BLE: power notify enabled method=UUID_DESCRIPTOR\r\n", gatt.out.c_str());

  gatt.subscribeOk = false;
  TEST_ASSERT_FALSE(gr3.enablePowerNotify(gatt, error));
  TEST_ASSERT_EQUAL_STRING("BLE power notify subscription unavailable", error.c_str());
}

void testGr3CaptureRequestNeedsCaptureInTheModeList() {
  Gr3BleProtocol gr3;
  std::string error;

  FakeGattSession noCapture;
  noCapture.characteristicValues[kModeList] = {0x01, 0x01};  // [count=1, playback]
  TEST_ASSERT_FALSE(gr3.requestCaptureOperationMode(noCapture, error));
  TEST_ASSERT_EQUAL_STRING("BLE operation mode list does not offer CAPTURE", error.c_str());
  TEST_ASSERT_EQUAL_STRING("BLE: operation mode list=[01 01]\n", noCapture.out.c_str());

  FakeGattSession withCapture;
  withCapture.characteristicValues[kModeList] = {0x02, 0x01, 0x00};
  TEST_ASSERT_TRUE(gr3.requestCaptureOperationMode(withCapture, error));
  const std::string expected = "readRaw " + kModeList + " @" + kCameraService + " | write " + kMode +
                               " OperationMode [00] @" + kCameraService;
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(withCapture.calls).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "BLE: operation mode list=[02 01 00]\nBLE: operation mode write CAPTURE result=OK\r\n",
      withCapture.out.c_str());

  FakeGattSession noList;
  TEST_ASSERT_FALSE(gr3.requestCaptureOperationMode(noList, error));
  TEST_ASSERT_EQUAL_STRING("BLE operation mode list unavailable", error.c_str());

  FakeGattSession unauthenticated;
  unauthenticated.link.encrypted = false;
  TEST_ASSERT_FALSE(gr3.requestCaptureOperationMode(unauthenticated, error));
  TEST_ASSERT_EQUAL_STRING("BLE capture mode request requires authenticated encryption", error.c_str());
  TEST_ASSERT_TRUE(unauthenticated.calls.empty());
}

// ---- Wi-Fi credentials ----

std::vector<uint8_t> bytes(const char* text) {
  return std::vector<uint8_t>(text, text + std::strlen(text));
}

void testGr4ParsesThePlainHandleValues() {
  // The shape a GR IV returns per handle: text SSID / passphrase, a one-byte
  // security type and the frequency as a little-endian number.
  RicohWifiCredentials c;
  mergeGr4CredentialValue(c, "ssid", bytes("GR_CEC596"));
  TEST_ASSERT_FALSE(c.valid);
  mergeGr4CredentialValue(c, "passphrase", bytes("abcd1234"));
  TEST_ASSERT_TRUE(c.valid);
  mergeGr4CredentialValue(c, "security", {0x03});
  mergeGr4CredentialValue(c, "frequency", {0x6C, 0x09});  // 2412
  TEST_ASSERT_EQUAL_STRING("GR_CEC596", c.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("abcd1234", c.passphrase.c_str());
  TEST_ASSERT_EQUAL_INT(3, c.securityType);
  TEST_ASSERT_EQUAL_UINT16(2412, c.frequencyMhz);
  TEST_ASSERT_EQUAL_UINT8(1, c.channel);
  TEST_ASSERT_TRUE(c.bssid.empty());
}

void testGr4ParsesKeyValueText() {
  RicohWifiCredentials c;
  mergeGr4CredentialValue(c, "ssid",
                          bytes("{\"SSID\": \"GR_ABC123\", \"Password\"='pw 12345', \"bssid\":\"\"}"));
  TEST_ASSERT_EQUAL_STRING("GR_ABC123", c.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("pw 12345", c.passphrase.c_str());
  TEST_ASSERT_TRUE(c.valid);

  // A MAC anywhere in the text, dashes or colons, ends up upper case.
  mergeGr4CredentialValue(c, "security", bytes("ap f0-3e-05-ce-c5-97 wpa2"));
  TEST_ASSERT_EQUAL_STRING("F0:3E:05:CE:C5:97", c.bssid.c_str());
}

void testGr4OnlyAcceptsGrLookingSsids() {
  RicohWifiCredentials c;
  mergeGr4CredentialValue(c, "ssid", bytes("HomeWiFi"));
  TEST_ASSERT_TRUE(c.ssid.empty());
  mergeGr4CredentialValue(c, "ssid", bytes("ricoh_gr_x"));
  TEST_ASSERT_EQUAL_STRING("ricoh_gr_x", c.ssid.c_str());
  mergeGr4CredentialValue(c, "ssid", bytes("GR_0123456789012345678901234567890"));  // 33 chars
  TEST_ASSERT_EQUAL_STRING("ricoh_gr_x", c.ssid.c_str());
}

void testGr4FlagsAnUnreadablePassphrase() {
  RicohWifiCredentials c;
  mergeGr4CredentialValue(c, "ssid", bytes("GR_CEC596"));
  mergeGr4CredentialValue(c, "passphrase", {0x8F, 0x12, 0xA0});
  TEST_ASSERT_TRUE(c.encryptedPassphrase);
  TEST_ASSERT_FALSE(c.valid);

  // An open network (security 0) needs no passphrase.
  RicohWifiCredentials open;
  mergeGr4CredentialValue(open, "ssid", bytes("GR_OPEN"));
  mergeGr4CredentialValue(open, "security", {0x00});
  TEST_ASSERT_TRUE(open.valid);
}

void testGr4BssidAndFrequencyForms() {
  RicohWifiCredentials c;
  mergeGr4CredentialValue(c, "bssid", {0xF2, 0x3E, 0x05, 0xCE, 0xC6, 0x96});
  TEST_ASSERT_EQUAL_STRING("F2:3E:05:CE:C6:96", c.bssid.c_str());

  RicohWifiCredentials text;
  mergeGr4CredentialValue(text, "frequency", bytes("channel 6"));
  TEST_ASSERT_EQUAL_UINT16(2437, text.frequencyMhz);
  TEST_ASSERT_EQUAL_UINT8(6, text.channel);

  RicohWifiCredentials big;
  mergeGr4CredentialValue(big, "frequency", {0x09, 0xAE});  // 2478 BE, not a 2.4 GHz centre
  TEST_ASSERT_EQUAL_UINT16(0, big.frequencyMhz);
  mergeGr4CredentialValue(big, "frequency", {0x00, 0x00, 0x09, 0x9B});  // 2459 BE -> no
  TEST_ASSERT_EQUAL_UINT16(0, big.frequencyMhz);
  mergeGr4CredentialValue(big, "frequency", {0x09, 0xB4});  // 2484 BE -> channel 14
  TEST_ASSERT_EQUAL_UINT16(2484, big.frequencyMhz);
  TEST_ASSERT_EQUAL_UINT8(14, big.channel);
}

void testGr4TextHelpersKeepArduinoStringSemantics() {
  TEST_ASSERT_EQUAL_STRING("a b", gr4PrintableText(bytes("\t a\rb \n")).c_str());
  TEST_ASSERT_EQUAL_STRING("", gr4PrintableText({'a', 0x07}).c_str());
  TEST_ASSERT_EQUAL_STRING("x", gr4PrintableText({0, 'x', 0}).c_str());
  TEST_ASSERT_EQUAL_STRING("v1", gr4MaybeQuotedValue("Key : \"v1\" ,", "key").c_str());
  TEST_ASSERT_EQUAL_STRING("", gr4MaybeQuotedValue("key v1", "key").c_str());
  TEST_ASSERT_EQUAL_STRING("", gr4MacFromText("f0:3e:05:ce:c5").c_str());
  TEST_ASSERT_EQUAL_STRING("", gr4MacFromText("f0:3e-05:ce:c5:97").c_str());
  TEST_ASSERT_EQUAL_UINT16(2412, gr4FrequencyFromValue("ch=1", {}));
}

void testGr4PollReadsEveryHandleWithPauses() {
  Gr4LegacyBleProtocol gr4;
  FakeGattSession gatt;
  gatt.handleValues[RICOH_BLE_GR4_WLAN_SSID_HANDLE] = bytes("GR_CEC596");
  gatt.handleValues[RICOH_BLE_GR4_WLAN_PASSPHRASE_HANDLE] = bytes("abcd1234");
  RicohWifiCredentials c;
  std::string error;
  TEST_ASSERT_TRUE(gr4.readWifiCredentials(gatt, c, error));
  TEST_ASSERT_TRUE(c.valid);
  // Unreadable handles are skipped but still followed by the same pause.
  TEST_ASSERT_EQUAL_STRING(
      "readHandle 0x0138 | pause 20 | readHandle 0x013A | pause 20 | readHandle 0x013C | pause 20 | "
      "readHandle 0x013E | pause 20 | readHandle 0x0140 | pause 20",
      join(gatt.calls).c_str());
}

void testGr3PollReadsTheWlanCharacteristics() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  const std::string ssid = RICOH_BLE_GR3_WLAN_SSID_UUID;
  const std::string pass = RICOH_BLE_GR3_WLAN_PASSPHRASE_UUID;
  const std::string channel = RICOH_BLE_GR3_WLAN_CHANNEL_UUID;
  gatt.readResults[ssid] = RicohGattRead::Ok;
  gatt.characteristicValues[ssid] = {' ', 'G', 'R', '_', 'C', 'D', 'F', 'D', '6', '3', 0, 0};
  gatt.readResults[pass] = RicohGattRead::Ok;
  gatt.characteristicValues[pass] = bytes("secret12\n");
  gatt.readResults[channel] = RicohGattRead::Ok;
  gatt.characteristicValues[channel] = {6};
  RicohWifiCredentials c;
  c.encryptedPassphrase = true;
  std::string error;
  TEST_ASSERT_TRUE(gr3.readWifiCredentials(gatt, c, error));
  TEST_ASSERT_EQUAL_STRING("GR_CDFD63", c.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("secret12", c.passphrase.c_str());
  TEST_ASSERT_FALSE(c.encryptedPassphrase);
  TEST_ASSERT_EQUAL_UINT8(6, c.channel);
  TEST_ASSERT_TRUE(c.valid);
  const std::string expected = "read " + ssid + " ssid @" + kGr3Wlan + " | pause 20 | read " + pass +
                               " passphrase @" + kGr3Wlan + " | pause 20 | read " + channel + " channel @" +
                               kGr3Wlan + " | pause 20";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(gatt.calls).c_str());
}

void testGr3PollSkipsMissingValuesAndStopsOnAuthFailure() {
  Gr3BleProtocol gr3;
  const std::string ssid = RICOH_BLE_GR3_WLAN_SSID_UUID;
  const std::string pass = RICOH_BLE_GR3_WLAN_PASSPHRASE_UUID;
  std::string error;

  // Missing and failed reads are skipped without a pause; earlier passes
  // still count.
  FakeGattSession partial;
  partial.readResults[ssid] = RicohGattRead::Failed;
  RicohWifiCredentials c;
  c.ssid = "GR_KEEP";
  TEST_ASSERT_TRUE(gr3.readWifiCredentials(partial, c, error));
  TEST_ASSERT_EQUAL_STRING("GR_KEEP", c.ssid.c_str());
  TEST_ASSERT_FALSE(c.valid);
  TEST_ASSERT_EQUAL_UINT32(3, partial.calls.size());

  FakeGattSession rejected;
  rejected.readResults[ssid] = RicohGattRead::Ok;
  rejected.characteristicValues[ssid] = bytes("GR_X");
  rejected.readResults[pass] = RicohGattRead::Rejected;
  RicohWifiCredentials d;
  TEST_ASSERT_FALSE(gr3.readWifiCredentials(rejected, d, error));
  TEST_ASSERT_EQUAL_STRING("BLE protected read requires authenticated encryption", error.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, rejected.calls.size());  // read, pause, read; channel never read
}

// ---- Pairing and status notifications ----

void testGr3ArmsPairingWithAnUnwaitedSsidRead() {
  Gr3BleProtocol gr3;
  const std::string ssid = RICOH_BLE_GR3_WLAN_SSID_UUID;
  TEST_ASSERT_TRUE(gr3.triggerPairingWithProtectedRead());

  FakeGattSession started;
  TEST_ASSERT_TRUE(gr3.armPairing(started));
  const std::string expected = "startRead " + ssid + " @" + kGr3Wlan + " | pause 300";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(started.calls).c_str());
  TEST_ASSERT_EQUAL_STRING("BLE pairing: armed by protected SSID read\r\n", started.out.c_str());

  FakeGattSession refused;
  refused.startResult = RicohGattStart::Failed;
  refused.startRc = 14;
  TEST_ASSERT_FALSE(gr3.armPairing(refused));
  TEST_ASSERT_EQUAL_STRING("BLE pairing: protected SSID read start failed rc=14\n", refused.out.c_str());
  TEST_ASSERT_EQUAL_UINT32(1, refused.calls.size());  // no pause

  FakeGattSession missing;
  missing.startResult = RicohGattStart::Unavailable;
  TEST_ASSERT_FALSE(gr3.armPairing(missing));
  TEST_ASSERT_TRUE(missing.out.empty());
}

void testGr4NeedsNoPairingProvocation() {
  Gr4LegacyBleProtocol gr4;
  TEST_ASSERT_FALSE(gr4.triggerPairingWithProtectedRead());
  FakeGattSession gatt;
  TEST_ASSERT_FALSE(gr4.armPairing(gatt));
  TEST_ASSERT_TRUE(gatt.calls.empty());
}

void testAuthenticationProbeReadsTheGr3Ssid() {
  Gr3BleProtocol gr3;
  FakeGattSession gatt;
  gatt.probeRc = 261;
  TEST_ASSERT_EQUAL_INT(261, gr3.probeAuthenticationError(gatt, 5));
  const std::string expected = std::string("probeRead ") + RICOH_BLE_GR3_WLAN_SSID_UUID + " @" + kGr3Wlan;
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), join(gatt.calls).c_str());

  FakeGattSession missing;
  missing.probeFound = false;
  TEST_ASSERT_EQUAL_INT(5, gr3.probeAuthenticationError(missing, 5));

  Gr4LegacyBleProtocol gr4;
  FakeGattSession untouched;
  TEST_ASSERT_EQUAL_INT(5, gr4.probeAuthenticationError(untouched, 5));
  TEST_ASSERT_TRUE(untouched.calls.empty());
}

void testStatusNotifyNeedsAuthenticationOnGr3Only() {
  Gr3BleProtocol gr3;
  Gr4LegacyBleProtocol gr4;
  std::string error;
  FakeGattSession open;
  open.link = RicohLinkSecurity{};
  TEST_ASSERT_FALSE(gr3.statusNotifyAllowed(open, error));
  TEST_ASSERT_EQUAL_STRING("BLE status notify requires authenticated encryption", error.c_str());
  TEST_ASSERT_TRUE(gr4.statusNotifyAllowed(open, error));
  TEST_ASSERT_TRUE(error.empty());

  FakeGattSession secure;
  TEST_ASSERT_TRUE(gr3.statusNotifyAllowed(secure, error));
}

void testHandleRoutedPowerNotifyDecodesLikeTheOldConstants() {
  // The GAP handler now decodes routed notifications with the routing
  // model's decodePowerState(); for GR IV that must equal the old
  // ON 0x01 / OFF 0x00 constants and ignore everything else.
  Gr4LegacyBleProtocol gr4;
  for (int value = 0; value <= 0xFF; ++value) {
    bool on = false;
    bool off = false;
    const bool known = gr4.decodePowerState(static_cast<uint8_t>(value), on, off);
    TEST_ASSERT_EQUAL(value == RICOH_BLE_GR4_POWER_STATE_ON_VALUE, known && on);
    TEST_ASSERT_EQUAL(value == RICOH_BLE_GR4_POWER_STATE_OFF_VALUE, known && off);
  }
}

void testHexFormattingMatchesTheLogFormat() {
  const uint8_t bytes[] = {0x00, 0x0A, 0xFF};
  TEST_ASSERT_EQUAL_STRING("00 0A FF", ricohHexBytes(bytes, sizeof(bytes)).c_str());
  TEST_ASSERT_EQUAL_STRING("", ricohHexBytes(bytes, 0).c_str());
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testGr4WifiIsTheWlanPowerHandle);
  RUN_TEST(testGr4WifiIgnoresTheOperationModeGate);
  RUN_TEST(testGr4PowerIsReadAndSubscribedByHandle);
  RUN_TEST(testGr4NotifyFailureStopsRoutingTheHandle);
  RUN_TEST(testGr4HasNoCaptureModeRequest);
  RUN_TEST(testGr3WifiWritesNetworkType);
  RUN_TEST(testGr3WifiNeedsAnAuthenticatedLink);
  RUN_TEST(testGr3WifiHonoursTheOperationModeGate);
  RUN_TEST(testGr3WifiReportsAMissingCharacteristic);
  RUN_TEST(testGr3PowerReadOutcomes);
  RUN_TEST(testGr3PowerNotifySubscribesTheUuid);
  RUN_TEST(testGr3CaptureRequestNeedsCaptureInTheModeList);
  RUN_TEST(testGr4ParsesThePlainHandleValues);
  RUN_TEST(testGr4ParsesKeyValueText);
  RUN_TEST(testGr4OnlyAcceptsGrLookingSsids);
  RUN_TEST(testGr4FlagsAnUnreadablePassphrase);
  RUN_TEST(testGr4BssidAndFrequencyForms);
  RUN_TEST(testGr4TextHelpersKeepArduinoStringSemantics);
  RUN_TEST(testGr4PollReadsEveryHandleWithPauses);
  RUN_TEST(testGr3PollReadsTheWlanCharacteristics);
  RUN_TEST(testGr3PollSkipsMissingValuesAndStopsOnAuthFailure);
  RUN_TEST(testGr3ArmsPairingWithAnUnwaitedSsidRead);
  RUN_TEST(testGr4NeedsNoPairingProvocation);
  RUN_TEST(testAuthenticationProbeReadsTheGr3Ssid);
  RUN_TEST(testStatusNotifyNeedsAuthenticationOnGr3Only);
  RUN_TEST(testHandleRoutedPowerNotifyDecodesLikeTheOldConstants);
  RUN_TEST(testHexFormattingMatchesTheLogFormat);
  return UNITY_END();
}
