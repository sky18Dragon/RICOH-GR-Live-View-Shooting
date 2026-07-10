#include <unity.h>

#include "protocol/CameraDiscoveryRegistry.h"
#include "protocol/CameraProtocolRegistry.h"
#include "protocol/CameraProtocolRuntime.h"
#include "protocol/CameraProtocolSelection.h"
#include "protocol/CameraProtocolValidator.h"
#include "protocol/profiles/Gr4ProtocolProfile.h"

namespace {

void assertModel(rvf::CameraModel expected, rvf::CameraModel actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}

void assertValidationError(
    rvf::CameraProtocolValidationError expected,
    const rvf::CameraProtocolValidationResult& actual) {
    TEST_ASSERT_FALSE(actual.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected),
                          static_cast<int>(actual.error));
    TEST_ASSERT_NOT_NULL(actual.field);
}

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
    TEST_ASSERT_EQUAL_STRING("9A5ED1C5-74CC-4C50-B5B6-66A48E7CCFF1", profile.uuids.infoService);
    TEST_ASSERT_EQUAL_STRING("4B445988-CAA0-4DD3-941D-37B4F52ACA86", profile.uuids.cameraService);
    TEST_ASSERT_EQUAL_STRING("1452335A-EC7F-4877-B8AB-0F72E18BB295", profile.uuids.operationMode);
    TEST_ASSERT_EQUAL_STRING("9F00F387-8345-4BBC-8B92-B87B52E3091A", profile.uuids.shootingService);
    TEST_ASSERT_EQUAL_STRING("B29E6DE3-1AEC-48C1-9D05-02CEA57CE664", profile.uuids.shootingFlavor);
    TEST_ASSERT_EQUAL_STRING("559644B8-E0BC-4011-929B-5CF9199851E7", profile.uuids.operationRequest);
    TEST_ASSERT_EQUAL_STRING("0F291746-0C80-4726-87A7-3C501FD3B4B6", profile.uuids.controlService);
}

void testGr4ProtocolCapabilitiesMatchExistingFirmware() {
    const rvf::CameraCapabilities& capabilities =
        rvf::gr4ProtocolProfile().capabilities;

    TEST_ASSERT_TRUE(capabilities.hasWlanSecurity);
    TEST_ASSERT_TRUE(capabilities.hasWlanFrequency);
    TEST_ASSERT_FALSE(capabilities.hasWlanChannel);
    TEST_ASSERT_TRUE(capabilities.hasWlanBssid);
    TEST_ASSERT_TRUE(capabilities.hasPowerStateNotify);
    TEST_ASSERT_TRUE(capabilities.supportsBleShutter);
    TEST_ASSERT_TRUE(capabilities.supportsWifiLiveView);
}

void testRegistryOnlyEnablesValidatedGr4Protocol() {
    const rvf::CameraProtocolProfile* gr4 =
        rvf::CameraProtocolRegistry::find(rvf::CameraModel::RicohGr4Hdf);

    TEST_ASSERT_NOT_NULL(gr4);
    assertModel(rvf::CameraModel::RicohGr4Hdf, gr4->model);
    TEST_ASSERT_NULL(
        rvf::CameraProtocolRegistry::find(rvf::CameraModel::RicohGr3x));
    TEST_ASSERT_NULL(
        rvf::CameraProtocolRegistry::find(rvf::CameraModel::Unknown));
    TEST_ASSERT_EQUAL_size_t(1, rvf::CameraProtocolRegistry::profileCount());
    TEST_ASSERT_EQUAL_PTR(gr4, &rvf::CameraProtocolRegistry::profileAt(0));
}

void testDefaultProtocolIsAlwaysGr4() {
    const rvf::CameraProtocolProfile& profile =
        rvf::CameraProtocolRegistry::defaultProfile();

    assertModel(rvf::CameraModel::RicohGr4Hdf, profile.model);
    TEST_ASSERT_EQUAL_STRING("RICOH_GR_IV_HDF", profile.modelName);
    TEST_ASSERT_EQUAL_STRING("RICOH_GR_IV_HDF",
                             rvf::cameraModelName(profile.model));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::BlePairingMode::ExistingDefault),
        static_cast<int>(profile.pairingMode));
}

void testResolutionUsesRequestedRegisteredGr4WithoutFallback() {
    const rvf::CameraProtocolResolution resolution =
        rvf::CameraProtocolRegistry::resolve(rvf::CameraModel::RicohGr4Hdf);

    assertModel(rvf::CameraModel::RicohGr4Hdf, resolution.requestedModel);
    assertModel(rvf::CameraModel::RicohGr4Hdf, resolution.resolvedModel);
    TEST_ASSERT_FALSE(resolution.usedFallback);
    TEST_ASSERT_EQUAL_PTR(&rvf::gr4ProtocolProfile(), resolution.profile);
}

void testResolutionFallsBackFromReservedGr3xToGr4() {
    const rvf::CameraProtocolResolution resolution =
        rvf::CameraProtocolRegistry::resolve(rvf::CameraModel::RicohGr3x);

    assertModel(rvf::CameraModel::RicohGr3x, resolution.requestedModel);
    assertModel(rvf::CameraModel::RicohGr4Hdf, resolution.resolvedModel);
    TEST_ASSERT_TRUE(resolution.usedFallback);
}

void testResolutionFallsBackFromUnknownAndInvalidModelsToGr4() {
    const rvf::CameraProtocolResolution unknown =
        rvf::CameraProtocolRegistry::resolve(rvf::CameraModel::Unknown);
    const rvf::CameraProtocolResolution invalid =
        rvf::CameraProtocolRegistry::resolve(
            static_cast<rvf::CameraModel>(0xFF));

    assertModel(rvf::CameraModel::RicohGr4Hdf, unknown.resolvedModel);
    assertModel(rvf::CameraModel::RicohGr4Hdf, invalid.resolvedModel);
    TEST_ASSERT_TRUE(unknown.usedFallback);
    TEST_ASSERT_TRUE(invalid.usedFallback);
    assertModel(rvf::CameraModel::Unknown, rvf::cameraModelFromRaw(0xFF));
}

void testProtocolSelectionDefaultsToGr4AndRejectsUnregisteredModel() {
    rvf::CameraProtocolSelection selection;

    assertModel(rvf::CameraModel::RicohGr4Hdf, selection.cameraModel());
    TEST_ASSERT_TRUE(selection.select(rvf::CameraModel::RicohGr4Hdf));
    TEST_ASSERT_FALSE(selection.select(rvf::CameraModel::RicohGr3x));
    assertModel(rvf::CameraModel::RicohGr4Hdf, selection.cameraModel());
    TEST_ASSERT_EQUAL_PTR(&rvf::CameraProtocolRegistry::defaultProfile(),
                          &selection.protocol());
}

void testFallbackConnectionPersistsActiveSelectionModel() {
    const rvf::CameraProtocolResolution resolution =
        rvf::CameraProtocolRegistry::resolve(rvf::CameraModel::RicohGr3x);
    rvf::CameraProtocolSelection activeSelection;

    TEST_ASSERT_TRUE(activeSelection.select(resolution.resolvedModel));
    const rvf::CameraModel persistedModel = activeSelection.cameraModel();
    assertModel(resolution.resolvedModel, persistedModel);
    assertModel(rvf::CameraModel::RicohGr4Hdf, persistedModel);
}

void testDiscoveryRegistryEnumeratesAllRegisteredProfileServices() {
    TEST_ASSERT_EQUAL_size_t(4,
                             rvf::CameraDiscoveryRegistry::serviceUuidCount());
    TEST_ASSERT_EQUAL_STRING(
        rvf::gr4ProtocolProfile().uuids.infoService,
        rvf::CameraDiscoveryRegistry::serviceUuidAt(0));
    TEST_ASSERT_EQUAL_STRING(
        rvf::gr4ProtocolProfile().uuids.controlService,
        rvf::CameraDiscoveryRegistry::serviceUuidAt(3));
    TEST_ASSERT_NULL(rvf::CameraDiscoveryRegistry::serviceUuidAt(4));
}

void testCompleteGr4ProfilePassesValidation() {
    const rvf::CameraProtocolValidationResult validation =
        rvf::validateCameraProtocolProfile(rvf::gr4ProtocolProfile());

    TEST_ASSERT_TRUE(validation.valid);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraProtocolValidationError::None),
        static_cast<int>(validation.error));
    TEST_ASSERT_NULL(validation.field);
}

void testValidatorRejectsUnknownModelAndEmptyName() {
    rvf::CameraProtocolProfile profile = rvf::gr4ProtocolProfile();
    profile.model = rvf::CameraModel::Unknown;
    assertValidationError(
        rvf::CameraProtocolValidationError::UnknownModel,
        rvf::validateCameraProtocolProfile(profile));

    profile = rvf::gr4ProtocolProfile();
    profile.modelName = "";
    assertValidationError(
        rvf::CameraProtocolValidationError::MissingModelName,
        rvf::validateCameraProtocolProfile(profile));
}

void testValidatorRejectsMissingWifiHandles() {
    rvf::CameraProtocolProfile profile = rvf::gr4ProtocolProfile();
    profile.handles.wlanEnable = 0;
    assertValidationError(
        rvf::CameraProtocolValidationError::MissingWlanEnableHandle,
        rvf::validateCameraProtocolProfile(profile));

    profile = rvf::gr4ProtocolProfile();
    profile.handles.wlanSecurity = 0;
    assertValidationError(
        rvf::CameraProtocolValidationError::MissingWlanSecurityHandle,
        rvf::validateCameraProtocolProfile(profile));

    profile = rvf::gr4ProtocolProfile();
    profile.capabilities.hasWlanChannel = true;
    profile.handles.wlanChannel = 0;
    assertValidationError(
        rvf::CameraProtocolValidationError::MissingWlanChannelHandle,
        rvf::validateCameraProtocolProfile(profile));
}

void testValidatorRejectsMissingPowerNotifyCccd() {
    rvf::CameraProtocolProfile profile = rvf::gr4ProtocolProfile();
    profile.handles.powerStateCccd = 0;

    assertValidationError(
        rvf::CameraProtocolValidationError::MissingPowerStateCccdHandle,
        rvf::validateCameraProtocolProfile(profile));
}

void testValidatorRejectsMissingShutterUuid() {
    rvf::CameraProtocolProfile profile = rvf::gr4ProtocolProfile();
    profile.uuids.operationRequest = "";

    assertValidationError(
        rvf::CameraProtocolValidationError::MissingOperationRequestUuid,
        rvf::validateCameraProtocolProfile(profile));
}

void testExistingDefaultPairingIsSupportedButPasskeyEntryIsNot() {
    TEST_ASSERT_TRUE(rvf::isBlePairingModeSupported(
        rvf::BlePairingMode::ExistingDefault));
    TEST_ASSERT_FALSE(rvf::isBlePairingModeSupported(
        rvf::BlePairingMode::PasskeyEntry));
    TEST_ASSERT_TRUE(rvf::isBlePairingModeSupported(
        rvf::gr4ProtocolProfile().pairingMode));
}

void testBleActivityBlocksCameraModelChanges() {
    TEST_ASSERT_TRUE(rvf::canChangeCameraModelDuringBleActivity(false,
                                                                false,
                                                                false));
    TEST_ASSERT_FALSE(rvf::canChangeCameraModelDuringBleActivity(true,
                                                                 false,
                                                                 false));
    TEST_ASSERT_FALSE(rvf::canChangeCameraModelDuringBleActivity(false,
                                                                 true,
                                                                 false));
    TEST_ASSERT_FALSE(rvf::canChangeCameraModelDuringBleActivity(false,
                                                                 false,
                                                                 true));
}

void testUnsupportedWifiProfileCannotOpenOrReadCredentials() {
    rvf::CameraProtocolProfile profile = rvf::gr4ProtocolProfile();
    profile.capabilities.supportsWifiLiveView = false;

    TEST_ASSERT_FALSE(rvf::canOpenWifiWithProfile(profile));
    TEST_ASSERT_FALSE(rvf::canReadWifiCredentialsWithProfile(profile));

    profile = rvf::gr4ProtocolProfile();
    profile.handles.wlanEnable = 0;
    TEST_ASSERT_FALSE(rvf::canOpenWifiWithProfile(profile));
    profile = rvf::gr4ProtocolProfile();
    profile.handles.wlanSsid = 0;
    TEST_ASSERT_FALSE(rvf::canReadWifiCredentialsWithProfile(profile));
}

void testUnsupportedNotifyAndShutterProfilesAreRejected() {
    rvf::CameraProtocolProfile profile = rvf::gr4ProtocolProfile();
    profile.capabilities.hasPowerStateNotify = false;
    TEST_ASSERT_FALSE(rvf::canEnablePowerStateNotifyWithProfile(profile));

    profile = rvf::gr4ProtocolProfile();
    profile.capabilities.supportsBleShutter = false;
    TEST_ASSERT_FALSE(rvf::canShootWithProfile(profile));

    profile = rvf::gr4ProtocolProfile();
    profile.uuids.shootingFlavor = "";
    TEST_ASSERT_FALSE(rvf::canShootWithProfile(profile));
}

}  // namespace

void runCameraProtocolTests() {
    RUN_TEST(testGr4ProtocolValuesMatchExistingFirmware);
    RUN_TEST(testGr4ProtocolCapabilitiesMatchExistingFirmware);
    RUN_TEST(testRegistryOnlyEnablesValidatedGr4Protocol);
    RUN_TEST(testDefaultProtocolIsAlwaysGr4);
    RUN_TEST(testResolutionUsesRequestedRegisteredGr4WithoutFallback);
    RUN_TEST(testResolutionFallsBackFromReservedGr3xToGr4);
    RUN_TEST(testResolutionFallsBackFromUnknownAndInvalidModelsToGr4);
    RUN_TEST(testProtocolSelectionDefaultsToGr4AndRejectsUnregisteredModel);
    RUN_TEST(testFallbackConnectionPersistsActiveSelectionModel);
    RUN_TEST(testDiscoveryRegistryEnumeratesAllRegisteredProfileServices);
    RUN_TEST(testCompleteGr4ProfilePassesValidation);
    RUN_TEST(testValidatorRejectsUnknownModelAndEmptyName);
    RUN_TEST(testValidatorRejectsMissingWifiHandles);
    RUN_TEST(testValidatorRejectsMissingPowerNotifyCccd);
    RUN_TEST(testValidatorRejectsMissingShutterUuid);
    RUN_TEST(testExistingDefaultPairingIsSupportedButPasskeyEntryIsNot);
    RUN_TEST(testBleActivityBlocksCameraModelChanges);
    RUN_TEST(testUnsupportedWifiProfileCannotOpenOrReadCredentials);
    RUN_TEST(testUnsupportedNotifyAndShutterProfilesAreRejected);
}
