#include <unity.h>

#include "protocol/CameraProtocolRegistry.h"
#include "protocol/profiles/Gr4ProtocolProfile.h"

namespace {

void testGr4ProtocolValuesMatchExistingFirmware() {
    const rvf::CameraProtocolProfile& profile = rvf::gr4ProtocolProfile();

    TEST_ASSERT_EQUAL_HEX16(0x0135, profile.handles.wlanEnable);
    TEST_ASSERT_EQUAL_HEX16(0x0138, profile.handles.wlanSsid);
    TEST_ASSERT_EQUAL_HEX16(0x013A, profile.handles.wlanPassphrase);
    TEST_ASSERT_EQUAL_HEX16(0x013C, profile.handles.wlanSecurity);
    TEST_ASSERT_EQUAL_HEX16(0x013E, profile.handles.wlanFrequency);
    TEST_ASSERT_EQUAL_HEX16(0x0000, profile.handles.wlanChannel);
    TEST_ASSERT_EQUAL_HEX16(0x0140, profile.handles.wlanBssid);
    TEST_ASSERT_EQUAL_HEX16(0x00EB, profile.handles.powerState);
    TEST_ASSERT_EQUAL_HEX16(0x00EC, profile.handles.powerStateCccd);
    TEST_ASSERT_EQUAL_HEX8(0x01, profile.wlanEnableValue);
    TEST_ASSERT_EQUAL_HEX8(0x01, profile.powerOnValue);
    TEST_ASSERT_EQUAL_HEX8(0x00, profile.powerOffValue);
}

void testGr4ProtocolCapabilitiesMatchExistingFirmware() {
    const rvf::CameraCapabilities& capabilities = rvf::gr4ProtocolProfile().capabilities;

    TEST_ASSERT_TRUE(capabilities.hasWlanSecurity);
    TEST_ASSERT_TRUE(capabilities.hasWlanFrequency);
    TEST_ASSERT_FALSE(capabilities.hasWlanChannel);
    TEST_ASSERT_TRUE(capabilities.hasWlanBssid);
    TEST_ASSERT_TRUE(capabilities.hasPowerStateNotify);
    TEST_ASSERT_TRUE(capabilities.supportsBleShutter);
    TEST_ASSERT_TRUE(capabilities.supportsWifiLiveView);
}

void testRegistryOnlyEnablesGr4Protocol() {
    const rvf::CameraProtocolProfile* gr4 =
        rvf::CameraProtocolRegistry::find(rvf::CameraModel::RicohGr4Hdf);

    TEST_ASSERT_NOT_NULL(gr4);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::CameraModel::RicohGr4Hdf),
                          static_cast<int>(gr4->model));
    TEST_ASSERT_NULL(rvf::CameraProtocolRegistry::find(rvf::CameraModel::RicohGr3x));
    TEST_ASSERT_NULL(rvf::CameraProtocolRegistry::find(rvf::CameraModel::Unknown));
}

void testDefaultProtocolIsAlwaysGr4() {
    const rvf::CameraProtocolProfile& profile = rvf::CameraProtocolRegistry::defaultProfile();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::CameraModel::RicohGr4Hdf),
                          static_cast<int>(profile.model));
    TEST_ASSERT_EQUAL_STRING("RICOH_GR_IV_HDF", profile.modelName);
    TEST_ASSERT_EQUAL_STRING("RICOH_GR_IV_HDF", rvf::cameraModelName(profile.model));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::BlePairingMode::ExistingDefault),
                          static_cast<int>(profile.pairingMode));
}

}  // namespace

void runCameraProtocolTests() {
    RUN_TEST(testGr4ProtocolValuesMatchExistingFirmware);
    RUN_TEST(testGr4ProtocolCapabilitiesMatchExistingFirmware);
    RUN_TEST(testRegistryOnlyEnablesGr4Protocol);
    RUN_TEST(testDefaultProtocolIsAlwaysGr4);
}
