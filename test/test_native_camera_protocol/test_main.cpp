#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "gr2_provisioning_logic.h"
#include "gr2_shutter_policy.h"
#include "brands/ricoh/common/RicohBleProtocolRouter.h"
#include "camera_sleep_policy.h"

namespace {

void testCameraSleepAutoPowerOffWaitsForTimeout() {
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(true, 1000, 30999, 30000));
  TEST_ASSERT_TRUE(cameraSleepAutoPowerOffDue(true, 1000, 31000, 30000));
}

void testCameraSleepAutoPowerOffRequiresActiveSleep() {
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(false, 1000, 50000, 30000));
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(true, 1000, 50000, 0));
}

void testCameraSleepAutoPowerOffHandlesMillisWrap() {
  constexpr uint32_t enteredAt = 0xFFFFFF00U;
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(true, enteredAt, 0x000000F0U, 1000));
  TEST_ASSERT_TRUE(cameraSleepAutoPowerOffDue(true, enteredAt, 0x00000300U, 1000));
}

void testDetectsProtocolOnlyFromSafeEvidence() {
  ProtocolDetectionEvidence gr3;
  gr3.gattDiscoveryComplete = true;
  gr3.hasGr3WlanService = true;
  gr3.hasGr3NetworkTypeCharacteristic = true;
  gr3.hasGr3SsidCharacteristic = true;
  gr3.hasGr3PassphraseCharacteristic = true;
  gr3.hasCameraService = true;
  gr3.hasOperationModeCharacteristic = true;
  gr3.hasShootingService = true;
  gr3.hasShootingFlavorCharacteristic = true;
  gr3.hasOperationRequestCharacteristic = true;
  gr3.hasControlService = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(detectRicohProtocol(gr3)));

  ProtocolDetectionEvidence incompleteGr3;
  incompleteGr3.gattDiscoveryComplete = true;
  incompleteGr3.hasGr3WlanService = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Unknown),
                        static_cast<int>(detectRicohProtocol(incompleteGr3)));

  ProtocolDetectionEvidence interruptedGr4MisleadingAsGr3 = gr3;
  interruptedGr4MisleadingAsGr3.gattDiscoveryComplete = false;
  interruptedGr4MisleadingAsGr3.hasGr4PowerCharacteristicAtExpectedHandle = false;
  interruptedGr4MisleadingAsGr3.gr4ExpectedWlanCharacteristicCount = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(interruptedGr4MisleadingAsGr3)));

  ProtocolDetectionEvidence gr4;
  gr4.hasCameraService = true;
  gr4.hasOperationModeCharacteristic = true;
  gr4.hasShootingService = true;
  gr4.hasShootingFlavorCharacteristic = true;
  gr4.hasOperationRequestCharacteristic = true;
  gr4.hasControlService = true;
  gr4.hasGr4PowerCharacteristicAtExpectedHandle = true;
  gr4.gr4WlanHandlesInExpectedService = true;
  gr4.gr4KnownWlanUuidHandleMapping = true;
  gr4.gr4ExpectedWlanCharacteristicCount = 6;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(detectRicohProtocol(gr4)));

  ProtocolDetectionEvidence gr4WithSharedWlanUuids = gr4;
  gr4WithSharedWlanUuids.hasGr3WlanService = true;
  gr4WithSharedWlanUuids.hasGr3NetworkTypeCharacteristic = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(detectRicohProtocol(gr4WithSharedWlanUuids)));

  ProtocolDetectionEvidence crossServiceNumericCollision = gr4;
  crossServiceNumericCollision.gr4WlanHandlesInExpectedService = false;
  crossServiceNumericCollision.gr4KnownWlanUuidHandleMapping = false;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(crossServiceNumericCollision)));

  ProtocolDetectionEvidence fiveOfSixGr4Handles = gr4;
  fiveOfSixGr4Handles.gr4ExpectedWlanCharacteristicCount = 5;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(fiveOfSixGr4Handles)));

  ProtocolDetectionEvidence wrongKnownUuidMapping = gr4;
  wrongKnownUuidMapping.gr4KnownWlanUuidHandleMapping = false;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(wrongKnownUuidMapping)));

  ProtocolDetectionEvidence incompleteFixedHandleConflict = gr3;
  incompleteFixedHandleConflict.hasGr4PowerCharacteristicAtExpectedHandle = true;
  incompleteFixedHandleConflict.gr4WlanHandlesInExpectedService = false;
  incompleteFixedHandleConflict.gr4KnownWlanUuidHandleMapping = false;
  incompleteFixedHandleConflict.gr4ExpectedWlanCharacteristicCount = 3;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Unknown),
                        static_cast<int>(detectRicohProtocol(incompleteFixedHandleConflict)));
}

void testShutterPreparationRemainsGenerationSpecific() {
  const CameraProtocolProfile& gr3 =
      cameraProtocolProfile(RicohProtocolGeneration::Gr3Family);
  const CameraProtocolProfile& gr4 =
      cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);

  TEST_ASSERT_TRUE(gr3.capabilities.supportsBleShutter);
  TEST_ASSERT_FALSE(gr3.requiresLegacyShutterFlavorWrite);
  TEST_ASSERT_TRUE(gr4.capabilities.supportsBleShutter);
  TEST_ASSERT_TRUE(gr4.requiresLegacyShutterFlavorWrite);
}

void testSecurityProfilesKeepGr4LegacyFrozen() {
  const RicohSecurityProfile& gr3 =
      ricohSecurityProfile(RicohSecurityProfileId::Gr3Passkey);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleIoCapability::KeyboardDisplay),
                        static_cast<int>(gr3.ioCapability));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleOwnAddressMode::Public),
                        static_cast<int>(gr3.ownAddressMode));
  TEST_ASSERT_TRUE(gr3.distributeEncryptionKey);
  TEST_ASSERT_TRUE(gr3.distributeIdentityKey);
  TEST_ASSERT_TRUE(gr3.distributeSigningKey);
  TEST_ASSERT_FALSE(gr3.usesFixedPasskey);

  const RicohSecurityProfile& gr4 =
      ricohSecurityProfile(RicohSecurityProfileId::Gr4Legacy);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleIoCapability::DisplayYesNo),
                        static_cast<int>(gr4.ioCapability));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleOwnAddressMode::RpaPublicDefault),
                        static_cast<int>(gr4.ownAddressMode));
  TEST_ASSERT_TRUE(gr4.distributeEncryptionKey);
  TEST_ASSERT_TRUE(gr4.distributeIdentityKey);
  TEST_ASSERT_FALSE(gr4.distributeSigningKey);
  TEST_ASSERT_TRUE(gr4.usesFixedPasskey);
  TEST_ASSERT_EQUAL_UINT32(123456, gr4.fixedPasskey);
}

void testOnlyGr4DiscoveryCanReuseTheLegacyConnection() {
  TEST_ASSERT_TRUE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Unknown,
      RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_TRUE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Gr4Legacy,
      RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_FALSE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Unknown,
      RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_FALSE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Gr3Passkey,
      RicohProtocolGeneration::Gr4Family));
}

void testProtocolRouterSelectsExactlyOneImplementation() {
  RicohBleProtocolRouter router;
  TEST_ASSERT_FALSE(router.hasProtocol());
  TEST_ASSERT_FALSE(router.select(RicohProtocolGeneration::Unknown));
  TEST_ASSERT_FALSE(router.hasProtocol());

  TEST_ASSERT_TRUE(router.select(RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_TRUE(router.hasProtocol());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(router.generation()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohSecurityProfileId::Gr4Legacy),
                        static_cast<int>(router.securityProfile()));
  TEST_ASSERT_FALSE(router.protocol()->requireAuthenticatedBond());

  TEST_ASSERT_TRUE(router.select(RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(router.profile().generation));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohSecurityProfileId::Gr3Passkey),
                        static_cast<int>(router.securityProfile()));
  TEST_ASSERT_TRUE(router.protocol()->requireAuthenticatedBond());

  TEST_ASSERT_FALSE(router.select(RicohProtocolGeneration::Gr2Family));
  TEST_ASSERT_FALSE(router.hasProtocol());
}

// detectRicohProtocol() as it was before detection moved into the models
// (stage 6.3). Kept verbatim as the reference for the exhaustive check below.
RicohProtocolGeneration referenceDetectBeforeStage6(const ProtocolDetectionEvidence& evidence) {
  const bool gr3Evidence =
      evidence.gattDiscoveryComplete &&
      evidence.hasGr3WlanService &&
      evidence.hasGr3NetworkTypeCharacteristic &&
      evidence.hasGr3SsidCharacteristic &&
      evidence.hasGr3PassphraseCharacteristic &&
      evidence.hasCameraService &&
      evidence.hasOperationModeCharacteristic &&
      evidence.hasShootingService &&
      evidence.hasShootingFlavorCharacteristic &&
      evidence.hasOperationRequestCharacteristic &&
      evidence.hasControlService;
  const bool gr4Evidence =
      evidence.hasCameraService &&
      evidence.hasOperationModeCharacteristic &&
      evidence.hasShootingService &&
      evidence.hasShootingFlavorCharacteristic &&
      evidence.hasOperationRequestCharacteristic &&
      evidence.hasControlService &&
      evidence.hasGr4PowerCharacteristicAtExpectedHandle &&
      evidence.gr4WlanHandlesInExpectedService &&
      evidence.gr4KnownWlanUuidHandleMapping &&
      evidence.gr4ExpectedWlanCharacteristicCount == 6;
  const bool partialGr4FixedHandleEvidence =
      evidence.hasGr4PowerCharacteristicAtExpectedHandle ||
      evidence.gr4WlanHandlesInExpectedService ||
      evidence.gr4KnownWlanUuidHandleMapping ||
      evidence.gr4ExpectedWlanCharacteristicCount > 0;

  // GR IV also exposes the WLAN service/Network Type UUIDs used by GR III.
  // Never classify those shared UUIDs as GR III after an interrupted GATT
  // discovery. GR III requires a complete, coherent read-only profile.
  // GR IV requires all six WLAN handles inside the WLAN service, the Power
  // handle inside Camera service, and known UUIDs at their expected handles.
  if (gr4Evidence) {
    return RicohProtocolGeneration::Gr4Family;
  }
  if (gr3Evidence && !partialGr4FixedHandleEvidence) {
    return RicohProtocolGeneration::Gr3Family;
  }
  return RicohProtocolGeneration::Unknown;
}

void testPerModelDetectionMatchesTheOriginalRuleExhaustively() {
  // Every combination of the 15 boolean markers and the 0-6 handle count.
  uint32_t checked = 0;
  for (uint32_t bits = 0; bits < (1u << 15); ++bits) {
    for (uint8_t count = 0; count <= 6; ++count) {
      ProtocolDetectionEvidence e;
      e.gattDiscoveryComplete = bits & (1u << 0);
      e.hasGr3WlanService = bits & (1u << 1);
      e.hasGr3NetworkTypeCharacteristic = bits & (1u << 2);
      e.hasGr3SsidCharacteristic = bits & (1u << 3);
      e.hasGr3PassphraseCharacteristic = bits & (1u << 4);
      e.hasGr3ChannelCharacteristic = bits & (1u << 5);
      e.hasCameraService = bits & (1u << 6);
      e.hasOperationModeCharacteristic = bits & (1u << 7);
      e.hasShootingService = bits & (1u << 8);
      e.hasShootingFlavorCharacteristic = bits & (1u << 9);
      e.hasOperationRequestCharacteristic = bits & (1u << 10);
      e.hasControlService = bits & (1u << 11);
      e.hasGr4PowerCharacteristicAtExpectedHandle = bits & (1u << 12);
      e.gr4WlanHandlesInExpectedService = bits & (1u << 13);
      e.gr4KnownWlanUuidHandleMapping = bits & (1u << 14);
      e.gr4ExpectedWlanCharacteristicCount = count;
      const RicohProtocolGeneration expected = referenceDetectBeforeStage6(e);
      const RicohProtocolGeneration actual = detectRicohProtocol(e);
      if (expected != actual) {
        char message[64];
        std::snprintf(message, sizeof(message), "bits=0x%04X count=%u", static_cast<unsigned>(bits),
                      static_cast<unsigned>(count));
        TEST_FAIL_MESSAGE(message);
      }
      ++checked;
    }
  }
  TEST_ASSERT_EQUAL_UINT32(229376, checked);
}

void testUnknownAndGr2ProfilesBlockBleSideEffects() {
  const CameraProtocolProfile& unknown = cameraProtocolProfile(RicohProtocolGeneration::Unknown);
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::WifiActivation));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::WifiDeactivation));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::CameraPowerWrite));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::Shutter));

  const CameraProtocolProfile& gr2 = cameraProtocolProfile(RicohProtocolGeneration::Gr2Family);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiActivationMethod::ManualOnly),
                        static_cast<int>(gr2.wifiActivationMethod));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiCredentialMethod::ManualConfiguration),
                        static_cast<int>(gr2.wifiCredentialMethod));
  TEST_ASSERT_FALSE(gr2.capabilities.hasBle);
  TEST_ASSERT_TRUE(gr2.capabilities.supportsHttpLiveView);
  TEST_ASSERT_TRUE(gr2.capabilities.supportsHttpShutter);
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(gr2, BleSideEffect::WifiActivation));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(gr2, BleSideEffect::WifiDeactivation));
}

void testGr3AndGr4AllowProfileScopedWifiDeactivation() {
  TEST_ASSERT_TRUE(protocolAllowsBleSideEffect(
      cameraProtocolProfile(RicohProtocolGeneration::Gr3Family),
      BleSideEffect::WifiDeactivation));
  TEST_ASSERT_TRUE(protocolAllowsBleSideEffect(
      cameraProtocolProfile(RicohProtocolGeneration::Gr4Family),
      BleSideEffect::WifiDeactivation));
}

void testOperationModeSafetyIsGenerationSpecific() {
  const CameraProtocolProfile& gr3 = cameraProtocolProfile(RicohProtocolGeneration::Gr3Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, true));
  // A GR IIIx ready to shoot reports Other, not Capture.
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Other, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::BleStartup, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::PowerOffTransfer, true));
  // Unlike GR IV, GR III still refuses to act on a mode it could not read.
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, false));

  const CameraProtocolProfile& gr4 = cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Unknown, false));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::PowerOffTransfer, true));
}

void testGr3StartupModeWaitsInsteadOfSleeping() {
  const CameraProtocolProfile& gr3 = cameraProtocolProfile(RicohProtocolGeneration::Gr3Family);
  TEST_ASSERT_TRUE(operationModeWaitsForCameraReady(gr3, RicohCameraOperationMode::BleStartup, true));
  // A real power-off transfer and an unreadable mode keep their existing gates.
  TEST_ASSERT_FALSE(operationModeWaitsForCameraReady(gr3, RicohCameraOperationMode::PowerOffTransfer, true));
  TEST_ASSERT_FALSE(operationModeWaitsForCameraReady(gr3, RicohCameraOperationMode::BleStartup, false));
  TEST_ASSERT_FALSE(operationModeWaitsForCameraReady(gr3, RicohCameraOperationMode::Capture, true));

  const CameraProtocolProfile& gr4 = cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);
  TEST_ASSERT_FALSE(operationModeWaitsForCameraReady(gr4, RicohCameraOperationMode::BleStartup, true));
  const CameraProtocolProfile& unknown = cameraProtocolProfile(RicohProtocolGeneration::Unknown);
  TEST_ASSERT_FALSE(operationModeWaitsForCameraReady(unknown, RicohCameraOperationMode::BleStartup, true));
}

void testOperationModeListParsesCountPrefixedEntries() {
  const uint8_t offersCapture[] = {0x03, 0x02, 0x01, 0x00};
  TEST_ASSERT_TRUE(operationModeListContains(offersCapture, sizeof(offersCapture), RicohCameraOperationMode::Capture));
  TEST_ASSERT_TRUE(operationModeListContains(offersCapture, sizeof(offersCapture), RicohCameraOperationMode::Playback));
  TEST_ASSERT_FALSE(operationModeListContains(offersCapture, sizeof(offersCapture), RicohCameraOperationMode::Other));

  // Bytes after the declared count are padding, not offered modes.
  const uint8_t padded[] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_FALSE(operationModeListContains(padded, sizeof(padded), RicohCameraOperationMode::Capture));
  // A count larger than the payload must not read past the buffer.
  const uint8_t truncated[] = {0x05, 0x02};
  TEST_ASSERT_FALSE(operationModeListContains(truncated, sizeof(truncated), RicohCameraOperationMode::Capture));
  const uint8_t empty[] = {0x00};
  TEST_ASSERT_FALSE(operationModeListContains(empty, sizeof(empty), RicohCameraOperationMode::Capture));
  TEST_ASSERT_FALSE(operationModeListContains(nullptr, 0, RicohCameraOperationMode::Capture));
}

void testGr3WifiProbeOnlyWidensStartupMode() {
  const CameraProtocolProfile& gr3 = cameraProtocolProfile(RicohProtocolGeneration::Gr3Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifiProbe(gr3, RicohCameraOperationMode::BleStartup, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifiProbe(gr3, RicohCameraOperationMode::Other, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifiProbe(gr3, RicohCameraOperationMode::PowerOffTransfer, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifiProbe(gr3, RicohCameraOperationMode::BleStartup, false));
  TEST_ASSERT_FALSE(operationModeAllowsWifiProbe(gr3, RicohCameraOperationMode::Unknown, false));

  const CameraProtocolProfile& gr4 = cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);
  TEST_ASSERT_FALSE(operationModeAllowsWifiProbe(gr4, RicohCameraOperationMode::BleStartup, true));
  const CameraProtocolProfile& unknown = cameraProtocolProfile(RicohProtocolGeneration::Unknown);
  TEST_ASSERT_FALSE(operationModeAllowsWifiProbe(unknown, RicohCameraOperationMode::BleStartup, true));
}

void testHttpShutterIsClaimedForLandscapeGenerations() {
  // Landscape releases BLE and shoots over POST /v1/camera/shoot.
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr3Family)
                       .capabilities.supportsHttpShutter);
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr4Family)
                       .capabilities.supportsHttpShutter);
  // GR II's official GR Remote uses the camera/shoot HTTP endpoint family.
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr2Family)
                       .capabilities.supportsHttpShutter);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Unknown)
                        .capabilities.supportsHttpShutter);
}

void testHttpFocusSettingIsClaimedForGr3AndGr4() {
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr2Family)
                        .capabilities.supportsHttpFocusSetting);
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr3Family)
                       .capabilities.supportsHttpFocusSetting);
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr4Family)
                       .capabilities.supportsHttpFocusSetting);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Unknown)
                        .capabilities.supportsHttpFocusSetting);
}

void testGr2ShutterUsesOfficialRemotePaths() {
  TEST_ASSERT_EQUAL_STRING("/v1/camera/shoot?af=camera", GR2_SHUTTER_ONESHOT_PATH);
  TEST_ASSERT_EQUAL_STRING("/v1/camera/shoot/start?af=camera", GR2_SHUTTER_START_PATH);
  TEST_ASSERT_EQUAL_STRING("/v1/camera/shoot/finish", GR2_SHUTTER_FINISH_PATH);
}

void testGr2ShutterResponseSelectsOneShotOrFallback() {
  Gr2InitialShutterDecision decision =
      evaluateGr2InitialShutterResponse("{\"errMsg\":\"OK\"}");
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Gr2InitialShutterAction::Complete),
                        static_cast<int>(decision.action));
  TEST_ASSERT_TRUE(decision.errorMessage.empty());

  decision = evaluateGr2InitialShutterResponse("{\"errMsg\":\"Precondition Failed\"}");
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Gr2InitialShutterAction::PressAndRelease),
                        static_cast<int>(decision.action));
  TEST_ASSERT_TRUE(decision.errorMessage.empty());

  // Legacy firmware may complete with an empty response body.
  decision = evaluateGr2InitialShutterResponse("");
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Gr2InitialShutterAction::Complete),
                        static_cast<int>(decision.action));
}

void testGr2ShutterResponseRejectsCameraErrors() {
  const Gr2InitialShutterDecision decision =
      evaluateGr2InitialShutterResponse("{ \"errMsg\" : \"Camera Busy\" }");
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Gr2InitialShutterAction::Fail),
                        static_cast<int>(decision.action));
  TEST_ASSERT_EQUAL_STRING("GR II shoot failed: Camera Busy", decision.errorMessage.c_str());

  std::string phaseError;
  TEST_ASSERT_TRUE(gr2ShutterPhaseSucceeded("{\"errMsg\":\"OK\"}", "start", phaseError));
  TEST_ASSERT_TRUE(phaseError.empty());
  TEST_ASSERT_TRUE(gr2ShutterPhaseSucceeded("", "finish", phaseError));
  TEST_ASSERT_FALSE(gr2ShutterPhaseSucceeded(
      "{\"errMsg\":\"Invalid State\"}", "finish", phaseError));
  TEST_ASSERT_EQUAL_STRING("GR II shoot finish failed: Invalid State", phaseError.c_str());
}

void testGr2ProvisioningFormValidationAndManualOverride() {
  Gr2ProvisioningValidation validation = validateGr2ProvisioningForm(
      {"RICOH_SELECTED", "", "12345678", "GR-II-Setup-1234"});
  TEST_ASSERT_TRUE(validation.valid);
  TEST_ASSERT_EQUAL_STRING("RICOH_SELECTED", validation.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("12345678", validation.passphrase.c_str());

  validation = validateGr2ProvisioningForm(
      {"RICOH_SELECTED", "  RICOH_MANUAL  ", "", "GR-II-Setup-1234"});
  TEST_ASSERT_TRUE(validation.valid);
  TEST_ASSERT_EQUAL_STRING("RICOH_MANUAL", validation.ssid.c_str());
  TEST_ASSERT_TRUE(validation.passphrase.empty());

  TEST_ASSERT_FALSE(validateGr2ProvisioningForm(
      {"", "", "12345678", "GR-II-Setup-1234"}).valid);
  TEST_ASSERT_FALSE(validateGr2ProvisioningForm(
      {"GR-II-Setup-1234", "", "12345678", "GR-II-Setup-1234"}).valid);
  TEST_ASSERT_FALSE(validateGr2ProvisioningForm(
      {"RICOH_CAMERA", "", "1234567", "GR-II-Setup-1234"}).valid);
  TEST_ASSERT_FALSE(validateGr2ProvisioningForm(
      {"RICOH_CAMERA", "", std::string(64, 'x'), "GR-II-Setup-1234"}).valid);
  TEST_ASSERT_TRUE(validateGr2ProvisioningForm(
      {std::string(32, 'S'), "", std::string(63, 'x'), "GR-II-Setup-1234"}).valid);
  TEST_ASSERT_FALSE(validateGr2ProvisioningForm(
      {std::string(33, 'S'), "", "12345678", "GR-II-Setup-1234"}).valid);
}

void testGr2ScanCacheRendersRicohFirstAndEscapesHtml() {
  const Gr2NetworkOptions options = renderGr2NetworkOptions(
      {{"Cafe&Guest", -50},
       {"GR-II-Setup-1234", -10},
       {"RICOH_<123>'\"", -40},
       {"", -20}},
      "GR-II-Setup-1234",
      true);

  TEST_ASSERT_TRUE(options.ricohFound);
  TEST_ASSERT_EQUAL_UINT32(2, options.optionCount);
  const size_t ricohPosition = options.html.find("★ RICOH_&lt;123&gt;&#39;&quot;");
  const size_t cafePosition = options.html.find("Cafe&amp;Guest");
  TEST_ASSERT_NOT_EQUAL(std::string::npos, ricohPosition);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, cafePosition);
  TEST_ASSERT_TRUE(ricohPosition < cafePosition);
  TEST_ASSERT_EQUAL(std::string::npos, options.html.find("GR-II-Setup-1234"));
}

void testGr2ScanCacheShowsEmptyAndNonRicohWarnings() {
  Gr2NetworkOptions options = renderGr2NetworkOptions({}, "GR-II-Setup-1234", false);
  TEST_ASSERT_FALSE(options.ricohFound);
  TEST_ASSERT_EQUAL_UINT32(0, options.optionCount);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, options.html.find("未找到网络"));

  options = renderGr2NetworkOptions({{"Guest", -65}}, "GR-II-Setup-1234", true);
  TEST_ASSERT_FALSE(options.ricohFound);
  TEST_ASSERT_EQUAL_UINT32(1, options.optionCount);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, options.html.find("未识别到 RICOH_ 热点"));
}

void testGr2CredentialHandoffWaitsForResponseFlushAndHandlesMillisWrap() {
  TEST_ASSERT_FALSE(gr2CredentialsReadyForHandoff(false, true, 1000, 2200, 1200));
  TEST_ASSERT_FALSE(gr2CredentialsReadyForHandoff(true, false, 1000, 2200, 1200));
  TEST_ASSERT_FALSE(gr2CredentialsReadyForHandoff(true, true, 1000, 2199, 1200));
  TEST_ASSERT_TRUE(gr2CredentialsReadyForHandoff(true, true, 1000, 2200, 1200));

  constexpr uint32_t submittedAt = 0xFFFFFF00U;
  TEST_ASSERT_FALSE(gr2CredentialsReadyForHandoff(
      true, true, submittedAt, 0x00000300U, 1100));
  TEST_ASSERT_TRUE(gr2CredentialsReadyForHandoff(
      true, true, submittedAt, 0x00000350U, 1100));
}

void testLandscapeKeepsBleShutterForAllGenerations() {
  // GR III/IV keep BLE through LiveView and shoot over BLE; HTTP shutter in
  // landscape dropped the GR III link on capture. GR II has no BLE link to
  // release; an unknown camera keeps BLE so it never loses its only confirmed
  // shutter path.
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr3Family)
                        .capabilities.allowBlePreviewParking);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr4Family)
                        .capabilities.allowBlePreviewParking);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr2Family)
                        .capabilities.allowBlePreviewParking);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Unknown)
                        .capabilities.allowBlePreviewParking);
}

void testOnlyGr4TreatsCachedWifiChannelAsAGuess() {
  // GR IV moved its AP between channels across WLAN restarts; GR III kept one.
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr4Family)
                       .capabilities.wifiChannelChangesOnRestart);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr3Family)
                        .capabilities.wifiChannelChangesOnRestart);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr2Family)
                        .capabilities.wifiChannelChangesOnRestart);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Unknown)
                        .capabilities.wifiChannelChangesOnRestart);
}

void testGr3CredentialShapeAllowsOptionalChannel() {
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 0));
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 11));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "secret", 12));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("", "secret", 1));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "", 1));
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testCameraSleepAutoPowerOffWaitsForTimeout);
  RUN_TEST(testCameraSleepAutoPowerOffRequiresActiveSleep);
  RUN_TEST(testCameraSleepAutoPowerOffHandlesMillisWrap);
  RUN_TEST(testDetectsProtocolOnlyFromSafeEvidence);
  RUN_TEST(testShutterPreparationRemainsGenerationSpecific);
  RUN_TEST(testSecurityProfilesKeepGr4LegacyFrozen);
  RUN_TEST(testOnlyGr4DiscoveryCanReuseTheLegacyConnection);
  RUN_TEST(testProtocolRouterSelectsExactlyOneImplementation);
  RUN_TEST(testPerModelDetectionMatchesTheOriginalRuleExhaustively);
  RUN_TEST(testUnknownAndGr2ProfilesBlockBleSideEffects);
  RUN_TEST(testGr3AndGr4AllowProfileScopedWifiDeactivation);
  RUN_TEST(testOperationModeSafetyIsGenerationSpecific);
  RUN_TEST(testGr3StartupModeWaitsInsteadOfSleeping);
  RUN_TEST(testGr3WifiProbeOnlyWidensStartupMode);
  RUN_TEST(testOperationModeListParsesCountPrefixedEntries);
  RUN_TEST(testHttpShutterIsClaimedForLandscapeGenerations);
  RUN_TEST(testHttpFocusSettingIsClaimedForGr3AndGr4);
  RUN_TEST(testGr2ShutterUsesOfficialRemotePaths);
  RUN_TEST(testGr2ShutterResponseSelectsOneShotOrFallback);
  RUN_TEST(testGr2ShutterResponseRejectsCameraErrors);
  RUN_TEST(testGr2ProvisioningFormValidationAndManualOverride);
  RUN_TEST(testGr2ScanCacheRendersRicohFirstAndEscapesHtml);
  RUN_TEST(testGr2ScanCacheShowsEmptyAndNonRicohWarnings);
  RUN_TEST(testGr2CredentialHandoffWaitsForResponseFlushAndHandlesMillisWrap);
  RUN_TEST(testLandscapeKeepsBleShutterForAllGenerations);
  RUN_TEST(testOnlyGr4TreatsCachedWifiChannelAsAGuess);
  RUN_TEST(testGr3CredentialShapeAllowsOptionalChannel);
  return UNITY_END();
}
