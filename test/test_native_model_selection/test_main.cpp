#include <cstdint>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "brands/ricoh/RicohModelSelection.h"
#include "brands/ricoh/common/RicohBleProtocolRouter.h"
#include "camera/CameraRegistry.h"
#include "camera_protocol_profile.h"

// Runs in env:native (every model) and env:native-gr3 / env:native-gr4
// (one model); expectations follow the RVF_RICOH_* switches of the build.

namespace {

// Read-only discovery of a GR III family camera.
ProtocolDetectionEvidence gr3Camera() {
  ProtocolDetectionEvidence e;
  e.gattDiscoveryComplete = true;
  e.hasGr3WlanService = true;
  e.hasGr3NetworkTypeCharacteristic = true;
  e.hasGr3SsidCharacteristic = true;
  e.hasGr3PassphraseCharacteristic = true;
  e.hasGr3ChannelCharacteristic = true;
  e.hasCameraService = true;
  e.hasOperationModeCharacteristic = true;
  e.hasShootingService = true;
  e.hasShootingFlavorCharacteristic = true;
  e.hasOperationRequestCharacteristic = true;
  e.hasControlService = true;
  return e;
}

// A GR IV exposes every GR III marker plus its fixed handles.
ProtocolDetectionEvidence gr4Camera() {
  ProtocolDetectionEvidence e = gr3Camera();
  e.hasGr4PowerCharacteristicAtExpectedHandle = true;
  e.gr4WlanHandlesInExpectedService = true;
  e.gr4KnownWlanUuidHandleMapping = true;
  e.gr4ExpectedWlanCharacteristicCount = 6;
  return e;
}

uint32_t enabledModelCount() {
  return (RVF_RICOH_GR2 ? 1 : 0) + (RVF_RICOH_GR3 ? 1 : 0) + (RVF_RICOH_GR4 ? 1 : 0);
}

void testRegistryListsExactlyTheCompiledModels() {
  TEST_ASSERT_EQUAL_UINT32(enabledModelCount(), rvf::cameraModelCount());
  bool seen[5] = {false, false, false, false, false};
  for (size_t i = 0; i < rvf::cameraModelCount(); ++i) {
    const uint8_t id = rvf::cameraModelAt(i).id;
    TEST_ASSERT_TRUE(id < 5);
    TEST_ASSERT_FALSE(seen[id]);
    seen[id] = true;
  }
  TEST_ASSERT_EQUAL(RVF_RICOH_GR2 != 0, seen[static_cast<int>(RicohProtocolGeneration::Gr2Family)]);
  TEST_ASSERT_EQUAL(RVF_RICOH_GR3 != 0, seen[static_cast<int>(RicohProtocolGeneration::Gr3Family)]);
  TEST_ASSERT_EQUAL(RVF_RICOH_GR4 != 0, seen[static_cast<int>(RicohProtocolGeneration::Gr4Family)]);

  const RicohProtocolGeneration preselected =
      static_cast<RicohProtocolGeneration>(rvf::cameraModelAt(rvf::defaultCameraModelIndex()).id);
  if (RVF_RICOH_GR3) {
    TEST_ASSERT_EQUAL(static_cast<int>(RicohProtocolGeneration::Gr3Family), static_cast<int>(preselected));
  } else {
    TEST_ASSERT_EQUAL(static_cast<int>(rvf::cameraModelAt(0).id), static_cast<int>(preselected));
  }
}

void testOnlyCompiledModelsCanBeSelected() {
  RicohBleProtocolRouter router;
  TEST_ASSERT_EQUAL(RVF_RICOH_GR3 != 0, router.select(RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_EQUAL(RVF_RICOH_GR4 != 0, router.select(RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_FALSE(router.select(RicohProtocolGeneration::Gr2Family));  // no BLE protocol
}

void testMissingModelsGetTheInertUnknownProfile() {
  const RicohProtocolGeneration generations[] = {RicohProtocolGeneration::Gr2Family,
                                                 RicohProtocolGeneration::Gr3Family,
                                                 RicohProtocolGeneration::Gr4Family};
  const bool enabled[] = {RVF_RICOH_GR2 != 0, RVF_RICOH_GR3 != 0, RVF_RICOH_GR4 != 0};
  for (size_t i = 0; i < 3; ++i) {
    const CameraProtocolProfile& profile = cameraProtocolProfile(generations[i]);
    if (enabled[i]) {
      TEST_ASSERT_EQUAL(static_cast<int>(generations[i]), static_cast<int>(profile.generation));
    } else {
      TEST_ASSERT_EQUAL(static_cast<int>(RicohProtocolGeneration::Unknown), static_cast<int>(profile.generation));
      TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(profile, BleSideEffect::WifiActivation));
      TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(profile, BleSideEffect::Shutter));
      // Nor does a missing GR II leave its Wi-Fi-only paths reachable.
      TEST_ASSERT_FALSE(cameraIsWifiOnly(profile));
      TEST_ASSERT_FALSE(cameraNeedsManualWifiProvisioning(profile));
      TEST_ASSERT_NULL(profile.fixedModelLabel);
      TEST_ASSERT_EQUAL(static_cast<int>(RicohSecurityProfileId::Unknown),
                        static_cast<int>(securityProfileForGeneration(generations[i])));
    }
  }
  TEST_ASSERT_EQUAL(static_cast<int>(RVF_RICOH_GR3 ? RicohSecurityProfileId::Gr3Passkey : RicohSecurityProfileId::Unknown),
                    static_cast<int>(ricohSecurityProfile(RicohSecurityProfileId::Gr3Passkey).id));
  TEST_ASSERT_EQUAL(static_cast<int>(RVF_RICOH_GR4 ? RicohSecurityProfileId::Gr4Legacy : RicohSecurityProfileId::Unknown),
                    static_cast<int>(ricohSecurityProfile(RicohSecurityProfileId::Gr4Legacy).id));
}

void testCamerasOfMissingModelsAreRejectedNotMisidentified() {
  // A GR IV carries every GR III marker. A firmware without GR IV must still
  // recognise it as GR IV -- and then refuse it -- rather than drive it as a
  // GR III.
  TEST_ASSERT_EQUAL(static_cast<int>(RVF_RICOH_GR4 ? RicohProtocolGeneration::Gr4Family
                                                   : RicohProtocolGeneration::Unknown),
                    static_cast<int>(detectRicohProtocol(gr4Camera())));
  TEST_ASSERT_EQUAL(static_cast<int>(RVF_RICOH_GR3 ? RicohProtocolGeneration::Gr3Family
                                                   : RicohProtocolGeneration::Unknown),
                    static_cast<int>(detectRicohProtocol(gr3Camera())));

  // Half a GR IV still vetoes GR III, in every build.
  ProtocolDetectionEvidence partial = gr3Camera();
  partial.hasGr4PowerCharacteristicAtExpectedHandle = true;
  TEST_ASSERT_EQUAL(static_cast<int>(RicohProtocolGeneration::Unknown),
                    static_cast<int>(detectRicohProtocol(partial)));
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testRegistryListsExactlyTheCompiledModels);
  RUN_TEST(testOnlyCompiledModelsCanBeSelected);
  RUN_TEST(testMissingModelsGetTheInertUnknownProfile);
  RUN_TEST(testCamerasOfMissingModelsAreRejectedNotMisidentified);
  return UNITY_END();
}
