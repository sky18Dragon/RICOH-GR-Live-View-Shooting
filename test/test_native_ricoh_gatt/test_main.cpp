#include <cstdio>
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

  bool subscribe(const char* service, const char* uuid, RicohNotifyTarget) override {
    calls.push_back(std::string("subscribe ") + uuid + " @" + service);
    return subscribeOk;
  }

  void routeHandleNotify(uint16_t handle, RicohNotifyTarget) override {
    calls.push_back("route " + handleText(handle));
    routedHandle = handle;
  }

  RicohLinkSecurity security() const override { return link; }
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
  RUN_TEST(testHexFormattingMatchesTheLogFormat);
  return UNITY_END();
}
