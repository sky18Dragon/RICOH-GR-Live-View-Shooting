#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "ble_reconnect_policy.h"

#include "camera_identity.h"
#include "camera_profile_schema.h"
#include "camera_protocol_profile.h"
#include "mjpeg_stream.h"
#include "supervisor/SystemSupervisor.h"

namespace {

struct CapturedFrames {
  uint32_t callbacks = 0;
  std::vector<uint8_t> lastFrame;
};

void captureFrame(const uint8_t* data, size_t len, void* user) {
  auto* captured = static_cast<CapturedFrames*>(user);
  ++captured->callbacks;
  captured->lastFrame.assign(data, data + len);
}

void assertDerivedBleName(const char* ssid, const char* expected) {
  const std::string actual = deriveBleNameFromWifiSsid(ssid);
  TEST_ASSERT_EQUAL_STRING(expected, actual.c_str());
}

void testBeginRejectsInvalidInputs() {
  MjpegStream stream;
  uint8_t buffer[32] = {};
  CapturedFrames captured;

  TEST_ASSERT_FALSE(stream.begin(nullptr, sizeof(buffer), captureFrame, &captured));
  TEST_ASSERT_FALSE(stream.begin(buffer, 3, captureFrame, &captured));
  TEST_ASSERT_FALSE(stream.begin(buffer, sizeof(buffer), nullptr, &captured));
  TEST_ASSERT_EQUAL_size_t(0, stream.process(buffer, sizeof(buffer)));
}

void testDeliversFrameSplitAcrossChunks() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t prefix[] = {0x00, 0x11, 0xFF};
  const uint8_t middle[] = {
      0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF,
  };
  const uint8_t suffix[] = {0xD9, 0x22};
  const uint8_t expected[] = {
      0xFF, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0xD9,
  };

  TEST_ASSERT_EQUAL_size_t(0, stream.process(prefix, sizeof(prefix)));
  TEST_ASSERT_EQUAL_size_t(0, stream.process(middle, sizeof(middle)));
  TEST_ASSERT_EQUAL_size_t(1, stream.process(suffix, sizeof(suffix)));

  TEST_ASSERT_EQUAL_UINT32(1, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(1, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(0, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(sizeof(expected), captured.lastFrame.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, captured.lastFrame.data(), sizeof(expected));
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDropsShortFrame() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t shortFrame[] = {0xFF, 0xD8, 0x01, 0xFF, 0xD9};

  TEST_ASSERT_EQUAL_size_t(0, stream.process(shortFrame, sizeof(shortFrame)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDropsOverflowFrameWhenEoiArrives() {
  MjpegStream stream;
  uint8_t buffer[8] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t frame[] = {
      0xFF, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0xD9,
  };

  TEST_ASSERT_EQUAL_size_t(0, stream.process(frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testResetClearsPartialFrameOnly() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t shortFrame[] = {0xFF, 0xD8, 0x01, 0xFF, 0xD9};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(shortFrame, sizeof(shortFrame)));

  const uint8_t partial[] = {0xFF, 0xD8, 0x01, 0x02};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(partial, sizeof(partial)));
  TEST_ASSERT_GREATER_THAN_size_t(0, stream.currentLength());

  stream.reset();

  const uint8_t suffix[] = {0x03, 0xFF, 0xD9};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(suffix, sizeof(suffix)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDerivesBleNameFromRicohWifiSsid() {
  assertDerivedBleName("GR_H264456", "GR_H264457");
  assertDerivedBleName("GR_000099", "GR_000100");
  assertDerivedBleName("GR_H009999", "GR_H010000");
}

void testLeavesNonNumericRicohWifiSsidUnchanged() {
  assertDerivedBleName("GR_", "GR_");
  assertDerivedBleName("GR_H", "GR_H");
  assertDerivedBleName("GR_HABC123", "GR_HABC123");
}

void testRejectsNonRicohWifiSsidForBleName() {
  assertDerivedBleName("", "");
  assertDerivedBleName("RICOH_GR", "");
  assertDerivedBleName("XGR_H264456", "");
}

void testRequiresBleAddressAndAddressTypeForDirectReconnect() {
  TEST_ASSERT_TRUE(hasDirectBleReconnectIdentity("aa:bb:cc:dd:ee:ff", true));
  TEST_ASSERT_FALSE(hasDirectBleReconnectIdentity("aa:bb:cc:dd:ee:ff", false));
  TEST_ASSERT_FALSE(hasDirectBleReconnectIdentity("", true));
  TEST_ASSERT_FALSE(hasDirectBleReconnectIdentity(nullptr, true));
}

void testBleCandidateDiscoveryIsOpenWithoutStoredIdentity() {
  TEST_ASSERT_TRUE(bleCandidateMatchesStoredIdentity("", "34:90:ea:cc:87:35"));
  TEST_ASSERT_TRUE(bleCandidateMatchesStoredIdentity(nullptr, "34:90:ea:cc:87:35"));
}

void testBleCandidateMustMatchStoredIdentity() {
  TEST_ASSERT_TRUE(bleCandidateMatchesStoredIdentity("34:90:EA:CC:87:35",
                                                    "34:90:ea:cc:87:35"));
  TEST_ASSERT_FALSE(bleCandidateMatchesStoredIdentity("34:90:ea:cc:87:35",
                                                     "f0:3e:05:26:44:57"));
  TEST_ASSERT_FALSE(bleCandidateMatchesStoredIdentity("34:90:ea:cc:87:35", ""));
  TEST_ASSERT_FALSE(bleCandidateMatchesStoredIdentity("34:90:ea:cc:87:35", nullptr));
}

rvf::SystemHealthSnapshot healthyPreviewSnapshot() {
  rvf::SystemHealthSnapshot snapshot;
  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.bleConnected = true;
  snapshot.wifiConnected = true;
  snapshot.previewRunning = true;
  snapshot.liveviewEnabled = true;
  snapshot.lastFrameAt = 1000;
  snapshot.lastLiveViewActivityAt = 1000;
  snapshot.liveViewStallTimeoutMs = 5000;
  return snapshot;
}

void testSupervisorWaitsForIntervalAndIgnoresHealthyPreview() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  supervisor.begin(1000);

  TEST_ASSERT_FALSE(supervisor.check(1999, healthyPreviewSnapshot(), message));
  TEST_ASSERT_FALSE(supervisor.check(2000, healthyPreviewSnapshot(), message));
}

void testSupervisorReportsPreviewClosed() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.previewRunning = false;
  supervisor.begin(0);

  TEST_ASSERT_TRUE(supervisor.check(1000, snapshot, message));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppEventType::PreviewStopped), static_cast<int>(message.type));
  TEST_ASSERT_EQUAL_STRING("supervisor preview closed", message.detail);
}

void testSupervisorIgnoresCameraSleepGuard() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.previewRunning = false;
  snapshot.cameraSleepGuardActive = true;
  supervisor.begin(0);

  TEST_ASSERT_FALSE(supervisor.check(1000, snapshot, message));
}

void testSupervisorReportsPreviewIdleTimeout() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.lastFrameAt = 1000;
  snapshot.lastLiveViewActivityAt = 1000;
  snapshot.liveViewStallTimeoutMs = 5000;
  supervisor.begin(0);

  TEST_ASSERT_TRUE(supervisor.check(7001, snapshot, message));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppEventType::PreviewTimeout), static_cast<int>(message.type));
  TEST_ASSERT_EQUAL_INT(6001, message.code);
  TEST_ASSERT_EQUAL_STRING("supervisor preview frame idle", message.detail);
}

void testSupervisorReportsFrameStallDespiteIncomingBytes() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.lastFrameAt = 1000;
  snapshot.lastLiveViewActivityAt = 6500;
  snapshot.liveViewStallTimeoutMs = 5000;
  supervisor.begin(0);

  TEST_ASSERT_TRUE(supervisor.check(7001, snapshot, message));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppEventType::PreviewTimeout), static_cast<int>(message.type));
  TEST_ASSERT_EQUAL_INT(6001, message.code);
  TEST_ASSERT_EQUAL_STRING("supervisor preview frame idle", message.detail);
}

void testDetectsProtocolOnlyFromSafeEvidence() {
  ProtocolDetectionEvidence gr3;
  gr3.hasGr3WlanService = true;
  gr3.hasGr3NetworkTypeCharacteristic = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(detectRicohProtocol(gr3)));

  ProtocolDetectionEvidence incompleteGr3;
  incompleteGr3.hasGr3WlanService = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Unknown),
                        static_cast<int>(detectRicohProtocol(incompleteGr3)));

  ProtocolDetectionEvidence gr4;
  gr4.gr4PowerHandleReadSucceeded = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(detectRicohProtocol(gr4)));
}

void testUnknownAndGr2ProfilesBlockBleSideEffects() {
  const CameraProtocolProfile& unknown = cameraProtocolProfile(RicohProtocolGeneration::Unknown);
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::WifiActivation));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::CameraPowerWrite));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::Shutter));

  const CameraProtocolProfile& gr2 = cameraProtocolProfile(RicohProtocolGeneration::Gr2Family);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiActivationMethod::ManualOnly),
                        static_cast<int>(gr2.wifiActivationMethod));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiCredentialMethod::ManualConfiguration),
                        static_cast<int>(gr2.wifiCredentialMethod));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(gr2, BleSideEffect::WifiActivation));
}

void testOperationModeSafetyIsGenerationSpecific() {
  const CameraProtocolProfile& gr3 = cameraProtocolProfile(RicohProtocolGeneration::Gr3Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::BleStartup, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, false));

  const CameraProtocolProfile& gr4 = cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Unknown, false));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::PowerOffTransfer, true));
}

void testGr3CredentialShapeAllowsOptionalChannel() {
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 0));
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 11));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "secret", 12));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("", "secret", 1));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "", 1));
}

void testOldProfileMetadataUpgradesWithoutAssumingProtocol() {
  StoredCameraProfileMetadata old;
  old.schemaVersion = 3;
  old.legacyWifiValid = true;
  const CameraProfileMetadata decoded = decodeCameraProfileMetadata(old);
  TEST_ASSERT_EQUAL_UINT32(CAMERA_PROFILE_SCHEMA_VERSION, decoded.schemaVersion);
  TEST_ASSERT_FALSE(decoded.protocolGenerationKnown);
  TEST_ASSERT_TRUE(decoded.wifiCredentialsValid);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiCredentialSource::Unknown),
                        static_cast<int>(decoded.wifiSource));
}

void testNewProfileMetadataRoundTrips() {
  CameraProfileMetadata metadata;
  metadata.protocolGeneration = static_cast<uint8_t>(RicohProtocolGeneration::Gr3Family);
  metadata.protocolGenerationKnown = true;
  metadata.capabilityVersion = CAMERA_CAPABILITY_SCHEMA_VERSION;
  metadata.wifiSource = WifiCredentialSource::BleUuidCharacteristics;
  metadata.wifiCredentialsValid = true;

  const StoredCameraProfileMetadata stored = encodeCameraProfileMetadata(metadata);
  const CameraProfileMetadata decoded = decodeCameraProfileMetadata(stored);
  TEST_ASSERT_TRUE(decoded.protocolGenerationKnown);
  TEST_ASSERT_EQUAL_UINT8(metadata.protocolGeneration, decoded.protocolGeneration);
  TEST_ASSERT_EQUAL_UINT16(metadata.capabilityVersion, decoded.capabilityVersion);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(metadata.wifiSource), static_cast<int>(decoded.wifiSource));
  TEST_ASSERT_TRUE(decoded.wifiCredentialsValid);
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testDetectsProtocolOnlyFromSafeEvidence);
  RUN_TEST(testUnknownAndGr2ProfilesBlockBleSideEffects);
  RUN_TEST(testOperationModeSafetyIsGenerationSpecific);
  RUN_TEST(testGr3CredentialShapeAllowsOptionalChannel);
  RUN_TEST(testOldProfileMetadataUpgradesWithoutAssumingProtocol);
  RUN_TEST(testNewProfileMetadataRoundTrips);
  RUN_TEST(testBeginRejectsInvalidInputs);
  RUN_TEST(testDeliversFrameSplitAcrossChunks);
  RUN_TEST(testDropsShortFrame);
  RUN_TEST(testDropsOverflowFrameWhenEoiArrives);
  RUN_TEST(testResetClearsPartialFrameOnly);
  RUN_TEST(testDerivesBleNameFromRicohWifiSsid);
  RUN_TEST(testLeavesNonNumericRicohWifiSsidUnchanged);
  RUN_TEST(testRejectsNonRicohWifiSsidForBleName);
  RUN_TEST(testRequiresBleAddressAndAddressTypeForDirectReconnect);
  RUN_TEST(testBleCandidateDiscoveryIsOpenWithoutStoredIdentity);
  RUN_TEST(testBleCandidateMustMatchStoredIdentity);
  RUN_TEST(testSupervisorWaitsForIntervalAndIgnoresHealthyPreview);
  RUN_TEST(testSupervisorReportsPreviewClosed);
  RUN_TEST(testSupervisorIgnoresCameraSleepGuard);
  RUN_TEST(testSupervisorReportsPreviewIdleTimeout);
  RUN_TEST(testSupervisorReportsFrameStallDespiteIncomingBytes);
  return UNITY_END();
}
