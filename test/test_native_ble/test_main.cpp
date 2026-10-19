#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "ble_pairing_policy.h"
#include "ble_disconnect_policy.h"
#include "ble_discovery_policy.h"
#include "ble_reconnect_policy.h"
#include "ble_scan_lifecycle_policy.h"

#include "camera_identity.h"
#include "ble_link_mode_policy.h"

namespace {

void assertDerivedBleName(const char* ssid, const char* expected) {
  const std::string actual = deriveBleNameFromWifiSsid(ssid);
  TEST_ASSERT_EQUAL_STRING(expected, actual.c_str());
}

void testBleDisconnectPolicyDistinguishesLocalParkFromCameraSleep() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleDisconnectKind::LocalHost),
                        static_cast<int>(classifyRicohBleDisconnect(0x216)));
  TEST_ASSERT_FALSE(ricohBleDisconnectMayIndicateCameraSleep(0x216));
  TEST_ASSERT_EQUAL_STRING("LOCAL_HOST_TERMINATED", ricohBleDisconnectReasonName(0x216));
}

void testBleDisconnectPolicyUsesCorrectEstablishmentFailureCode() {
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohBleDisconnectKind::ConnectionEstablishmentFailed),
      static_cast<int>(classifyRicohBleDisconnect(0x23E)));
  TEST_ASSERT_FALSE(ricohBleDisconnectMayIndicateCameraSleep(0x23E));
  TEST_ASSERT_EQUAL_STRING("CONNECTION_ESTABLISHMENT_FAILED",
                           ricohBleDisconnectReasonName(0x23E));
}

void testBleDisconnectPolicyKeepsOnlyTrustedSleepReasons() {
  TEST_ASSERT_TRUE(ricohBleDisconnectMayIndicateCameraSleep(0x213));
  TEST_ASSERT_TRUE(ricohBleDisconnectMayIndicateCameraSleep(0x215));
  TEST_ASSERT_FALSE(ricohBleDisconnectMayIndicateCameraSleep(0x208));
  TEST_ASSERT_FALSE(ricohBleDisconnectMayIndicateCameraSleep(0));
  TEST_ASSERT_FALSE(ricohBleDisconnectMayIndicateCameraSleep(0x299));
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

void testDirectReconnectIsOnlyUsedForKnownBondedBootProfile() {
  constexpr const char* address = "aa:bb:cc:dd:ee:ff";
  TEST_ASSERT_TRUE(shouldAttemptDirectBleReconnect(true, true, true, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(false, true, true, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(true, false, true, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(true, true, false, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(true, true, true, address, false));
}

void testProtocolDiscoveryRefreshesOnlyRequiredCharacteristicServices() {
  TEST_ASSERT_TRUE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::SharedWlan));
  TEST_ASSERT_TRUE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::Camera));
  TEST_ASSERT_TRUE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::Shooting));
  TEST_ASSERT_FALSE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::Control));
}

void testNewPeerMustPersistBondBeforeConnectionSucceeds() {
  constexpr unsigned long timeoutMs = 1000;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Wait),
      static_cast<int>(decideBleBondPersistence(false, true, false, 100, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Wait),
      static_cast<int>(decideBleBondPersistence(false, true, false, 300, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Wait),
      static_cast<int>(decideBleBondPersistence(false, true, false, 500, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Ready),
      static_cast<int>(decideBleBondPersistence(false, true, true, 500, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Ready),
      static_cast<int>(decideBleBondPersistence(true, true, false, timeoutMs, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Disconnected),
      static_cast<int>(decideBleBondPersistence(true, false, false, 0, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Disconnected),
      static_cast<int>(decideBleBondPersistence(false, false, false, 500, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::TimedOut),
      static_cast<int>(decideBleBondPersistence(false, true, false, timeoutMs, timeoutMs)));
}

void testBleScanCleanupRequiresHostAndCallbackQuiescence() {
  TEST_ASSERT_TRUE(canCleanupBleScanSession(false, false, false, false, false, true, false));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(false, false, false, false, false, false, true));

  // Natural completion requires the host's onScanEnd signal.
  TEST_ASSERT_TRUE(canCleanupBleScanSession(true, false, true, true, true, true, false));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, false, true, false, true, true, true));

  // NimBLE stop() does not emit onScanEnd; successful cancellation, an
  // inactive controller, callback quiescence and a host-queue fence are the
  // safe terminal state.
  TEST_ASSERT_TRUE(canCleanupBleScanSession(true, true, true, false, true, true, true));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, true, false, true, true, false));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, false, false, true, true, true));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, true, false, false, true, true));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, true, false, true, false, true));
}

void testBleStackObjectsClearOnlyAfterHostStops() {
  TEST_ASSERT_TRUE(canUseBleStack(false));
  TEST_ASSERT_FALSE(canUseBleStack(true));

  TEST_ASSERT_FALSE(canClearBleStackObjects(false));
  TEST_ASSERT_TRUE(canClearBleStackObjects(true));

  TEST_ASSERT_FALSE(canRestartBleStack(false, false, true));
  TEST_ASSERT_FALSE(canRestartBleStack(false, true, true));
  TEST_ASSERT_TRUE(canRestartBleStack(true, false, false));
  TEST_ASSERT_TRUE(canRestartBleStack(true, true, true));
  TEST_ASSERT_FALSE(canRestartBleStack(true, true, false));
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

void testBleLinkNormalModeMatchesConnectParams() {
  // Same values RicohBleClient::connect() hands NimBLE (30/50 ms, latency 1,
  // 5.12 s), so restoring Normal returns the link to its connect-time shape.
  const BleConnParams normal = bleConnParamsFor(BleLinkMode::Normal);
  TEST_ASSERT_EQUAL_UINT16(24, normal.minInterval);
  TEST_ASSERT_EQUAL_UINT16(40, normal.maxInterval);
  TEST_ASSERT_EQUAL_UINT16(1, normal.latency);
  TEST_ASSERT_EQUAL_UINT16(512, normal.supervisionTimeout);
  TEST_ASSERT_TRUE(bleConnParamsValid(normal));
}

void testBleLinkLiveViewModeIsBeaconAlignedAndValid() {
  const BleConnParams liveView = bleConnParamsFor(BleLinkMode::LiveViewLowDuty);
  TEST_ASSERT_EQUAL_UINT16(82, liveView.minInterval);  // 102.5 ms
  TEST_ASSERT_EQUAL_UINT16(82, liveView.maxInterval);
  TEST_ASSERT_EQUAL_UINT16(0, liveView.latency);       // shutter waits one interval at most
  TEST_ASSERT_EQUAL_UINT16(600, liveView.supervisionTimeout);
  TEST_ASSERT_TRUE(bleConnParamsValid(liveView));
  // Fewer connection events than Normal's fastest interval.
  TEST_ASSERT_TRUE(liveView.minInterval > bleConnParamsFor(BleLinkMode::Normal).maxInterval);
}

void testBleConnParamsValidationEnforcesSpecLimits() {
  BleConnParams params = bleConnParamsFor(BleLinkMode::LiveViewLowDuty);
  params.supervisionTimeout = 20;  // 200 ms <= 2 * 102.5 ms
  TEST_ASSERT_FALSE(bleConnParamsValid(params));
  params = bleConnParamsFor(BleLinkMode::LiveViewLowDuty);
  params.minInterval = 5;
  TEST_ASSERT_FALSE(bleConnParamsValid(params));
  params = bleConnParamsFor(BleLinkMode::LiveViewLowDuty);
  params.minInterval = 100;  // min > max
  TEST_ASSERT_FALSE(bleConnParamsValid(params));
  params = bleConnParamsFor(BleLinkMode::LiveViewLowDuty);
  params.latency = 500;
  TEST_ASSERT_FALSE(bleConnParamsValid(params));
}

void testBleLinkModeWaitsForStableLiveViewStream() {
  BleLinkModeInputs in;
  in.bleConnected = true;
  in.previewRequested = true;
  in.previewStreaming = true;
  in.streamingSinceMs = 10000;
  in.nowMs = 11499;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::Normal),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::Normal, in, 1500)));
  in.nowMs = 11500;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::LiveViewLowDuty),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::Normal, in, 1500)));
  // millis() wrap between stream start and now.
  in.streamingSinceMs = 0xFFFFFF00U;
  in.nowMs = 0x00000600U;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::LiveViewLowDuty),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::Normal, in, 1500)));
}

void testBleLinkModeHoldsDuringStreamGapWhilePreviewWanted() {
  // A LiveView recycle or recovery closes the stream briefly; the link keeps
  // its current mode instead of flapping parameters.
  BleLinkModeInputs in;
  in.bleConnected = true;
  in.previewRequested = true;
  in.previewStreaming = false;
  in.nowMs = 50000;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::LiveViewLowDuty),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::LiveViewLowDuty, in, 1500)));
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::Normal),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::Normal, in, 1500)));
  // A reopened stream that has not settled yet keeps low duty too.
  in.previewStreaming = true;
  in.streamingSinceMs = 49900;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::LiveViewLowDuty),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::LiveViewLowDuty, in, 1500)));
}

void testBleLinkModeReturnsToNormalWhenPreviewLeftOrBleDown() {
  BleLinkModeInputs in;
  in.bleConnected = true;
  in.previewRequested = false;  // portrait: BLE shutter scene
  in.previewStreaming = true;   // stream not closed yet this tick
  in.streamingSinceMs = 0;
  in.nowMs = 60000;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::Normal),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::LiveViewLowDuty, in, 1500)));
  in.previewRequested = true;
  in.bleConnected = false;
  TEST_ASSERT_EQUAL(static_cast<int>(BleLinkMode::Normal),
                    static_cast<int>(desiredBleLinkMode(BleLinkMode::LiveViewLowDuty, in, 1500)));
}

void testLockedBindingRejectsNewPairingAndOtherCameras() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PairingRequestDecision::Allow),
                        static_cast<int>(pairingRequestDecision(CameraBindingState::Pairing)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PairingRequestDecision::RejectAndInvalidate),
                        static_cast<int>(pairingRequestDecision(CameraBindingState::Locked)));
  TEST_ASSERT_TRUE(bindingStateAllowsCandidate(CameraBindingState::Locked,
                                               "AA:BB:CC:DD:EE:FF",
                                               "aa:bb:cc:dd:ee:ff"));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::Locked,
                                                "AA:BB:CC:DD:EE:FF",
                                                "11:22:33:44:55:66"));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::BondInvalid,
                                                "AA:BB:CC:DD:EE:FF",
                                                "AA:BB:CC:DD:EE:FF"));
}

void testPairingBindingAcceptsFirstValidCandidateWithoutStoredIdentity() {
  TEST_ASSERT_TRUE(bindingStateAllowsCandidate(CameraBindingState::Unpaired,
                                               "",
                                               "F0:3E:05:26:44:57"));
  TEST_ASSERT_TRUE(bindingStateAllowsCandidate(CameraBindingState::Pairing,
                                               "",
                                               "F0:3E:05:26:44:57"));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::Pairing,
                                                "",
                                                ""));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::Pairing,
                                                "",
                                                nullptr));
}

void testNormalizesResolvedPeerAddressTypes() {
  TEST_ASSERT_EQUAL_UINT8(0x00, normalizedPeerAddressType(0x00));
  TEST_ASSERT_EQUAL_UINT8(0x01, normalizedPeerAddressType(0x01));
  TEST_ASSERT_EQUAL_UINT8(0x00, normalizedPeerAddressType(0x02));
  TEST_ASSERT_EQUAL_UINT8(0x01, normalizedPeerAddressType(0x03));
}

void testPairingRecoveryCountsOnlyExplicitSecurityFailures() {
  PairingRecoveryPolicy policy;
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x208));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_TRUE(policy.onBondedSecurityFailure(0x213));
}

void testPairingLatencyPolicyAvoidsNestedUnbondedRetries() {
  TEST_ASSERT_EQUAL_UINT8(0, bleClientConnectRetries(false));
  TEST_ASSERT_EQUAL_UINT8(1, bleClientConnectRetries(true));
  TEST_ASSERT_EQUAL_UINT32(150, bleRetryDelayMs(true, 1000, 150));
  TEST_ASSERT_EQUAL_UINT32(1000, bleRetryDelayMs(false, 1000, 150));
}

void testPairingRecoveryDropsUnauthenticatedBondAfterTwoReads() {
  PairingRecoveryPolicy policy;
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x105));
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x101));
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x10F));
  TEST_ASSERT_TRUE(policy.onInsufficientAuthRead(0x108));
  policy.onAuthenticatedRead();
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x105));
}

void testPasskeySerialCollectorCompletesWithoutLoggingValue() {
  PasskeyDigitCollector collector;
  int32_t code = -1;
  const char* input = "2x5 6\r4:45";
  for (const char* c = input; *c != '\0'; ++c) {
    const int32_t result = collector.feed(*c);
    if (result >= 0) {
      code = result;
    }
  }
  TEST_ASSERT_EQUAL_INT32(256445, code);
  TEST_ASSERT_EQUAL_INT32(-1, collector.feed('1'));
}

void testPasskeyButtonEntryCompletesResetsAndTimesOut() {
  PasskeyButtonEntry entry;
  entry.start(1000, 45000);
  for (int i = 0; i < 5; ++i) {
    entry.shortPress();
  }
  TEST_ASSERT_EQUAL_UINT8(5, entry.digits()[0]);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Editing),
                        static_cast<int>(entry.confirmDigit()));
  for (int i = 0; i < 11; ++i) {
    entry.shortPress();
  }
  TEST_ASSERT_EQUAL_UINT8(1, entry.digits()[1]);
  for (int i = 0; i < 5; ++i) {
    entry.confirmDigit();
  }
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Complete),
                        static_cast<int>(entry.status(2000)));
  TEST_ASSERT_EQUAL_INT32(510000, entry.code());

  entry.start(5000, 100);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Editing),
                        static_cast<int>(entry.status(5099)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::TimedOut),
                        static_cast<int>(entry.status(5100)));
  entry.reset();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Idle),
                        static_cast<int>(entry.status(6000)));
  TEST_ASSERT_EQUAL_INT32(0, entry.code());
}

void testPasskeyButtonEntryReportsRemainingTime() {
  PasskeyButtonEntry entry;
  TEST_ASSERT_EQUAL_UINT32(0, entry.remainingMs(1000));  // inactive
  entry.start(1000, 28000);
  TEST_ASSERT_EQUAL_UINT32(28000, entry.remainingMs(1000));
  TEST_ASSERT_EQUAL_UINT32(18000, entry.remainingMs(11000));
  TEST_ASSERT_EQUAL_UINT32(0, entry.remainingMs(29000));     // expired
  TEST_ASSERT_EQUAL_UINT32(0, entry.remainingMs(40000));
  // millis() wrap inside the window.
  entry.start(0xFFFFF000U, 28000);
  TEST_ASSERT_EQUAL_UINT32(28000 - 0x2000U, entry.remainingMs(0x00001000U));
  // A submitted code no longer counts down.
  (void)entry.submit();
  TEST_ASSERT_EQUAL_UINT32(0, entry.remainingMs(0x00001000U));
}

void testPasskeyCanMoveWithBAndSubmitWithLongA() {
  PasskeyButtonEntry entry;
  entry.start(100, 1000);
  entry.shortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Editing),
                        static_cast<int>(entry.confirmDigit()));
  entry.shortPress();
  entry.shortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Complete),
                        static_cast<int>(entry.submit()));
  TEST_ASSERT_EQUAL_INT32(120000, entry.code());
  entry.reset();
  TEST_ASSERT_EQUAL_INT32(0, entry.code());
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testBleDisconnectPolicyDistinguishesLocalParkFromCameraSleep);
  RUN_TEST(testBleDisconnectPolicyUsesCorrectEstablishmentFailureCode);
  RUN_TEST(testBleDisconnectPolicyKeepsOnlyTrustedSleepReasons);
  RUN_TEST(testBleLinkNormalModeMatchesConnectParams);
  RUN_TEST(testBleLinkLiveViewModeIsBeaconAlignedAndValid);
  RUN_TEST(testBleConnParamsValidationEnforcesSpecLimits);
  RUN_TEST(testBleLinkModeWaitsForStableLiveViewStream);
  RUN_TEST(testBleLinkModeHoldsDuringStreamGapWhilePreviewWanted);
  RUN_TEST(testBleLinkModeReturnsToNormalWhenPreviewLeftOrBleDown);
  RUN_TEST(testNormalizesResolvedPeerAddressTypes);
  RUN_TEST(testPairingRecoveryCountsOnlyExplicitSecurityFailures);
  RUN_TEST(testPairingLatencyPolicyAvoidsNestedUnbondedRetries);
  RUN_TEST(testPairingRecoveryDropsUnauthenticatedBondAfterTwoReads);
  RUN_TEST(testPasskeySerialCollectorCompletesWithoutLoggingValue);
  RUN_TEST(testPasskeyButtonEntryCompletesResetsAndTimesOut);
  RUN_TEST(testPasskeyButtonEntryReportsRemainingTime);
  RUN_TEST(testPasskeyCanMoveWithBAndSubmitWithLongA);
  RUN_TEST(testLockedBindingRejectsNewPairingAndOtherCameras);
  RUN_TEST(testPairingBindingAcceptsFirstValidCandidateWithoutStoredIdentity);
  RUN_TEST(testDerivesBleNameFromRicohWifiSsid);
  RUN_TEST(testLeavesNonNumericRicohWifiSsidUnchanged);
  RUN_TEST(testRejectsNonRicohWifiSsidForBleName);
  RUN_TEST(testRequiresBleAddressAndAddressTypeForDirectReconnect);
  RUN_TEST(testDirectReconnectIsOnlyUsedForKnownBondedBootProfile);
  RUN_TEST(testProtocolDiscoveryRefreshesOnlyRequiredCharacteristicServices);
  RUN_TEST(testNewPeerMustPersistBondBeforeConnectionSucceeds);
  RUN_TEST(testBleScanCleanupRequiresHostAndCallbackQuiescence);
  RUN_TEST(testBleStackObjectsClearOnlyAfterHostStops);
  RUN_TEST(testBleCandidateDiscoveryIsOpenWithoutStoredIdentity);
  RUN_TEST(testBleCandidateMustMatchStoredIdentity);
  return UNITY_END();
}
