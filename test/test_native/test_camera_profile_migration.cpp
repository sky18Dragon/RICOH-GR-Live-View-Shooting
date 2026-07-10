#include <unity.h>

#include "protocol/CameraProfileMigration.h"

namespace {

void testCurrentProfileLoadsValidatedGr4Model() {
    TEST_ASSERT_EQUAL_UINT32(4, rvf::kCameraProfileVersion);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::RicohGr4Hdf),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, rvf::kCameraProfileVersion, 1)));
}

void testCurrentProfilePreservesReservedGr3xModel() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::RicohGr3x),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, rvf::kCameraProfileVersion, 2)));
}

void testMissingModelKeyMigratesToUnknown() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::Unknown),
        static_cast<int>(rvf::cameraModelFromStoredProfile(false, rvf::kCameraProfileVersion, 1)));
}

void testLegacyProfileVersionMigratesToUnknown() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::Unknown),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, 3, 1)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::Unknown),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, 3, 2)));
}

void testInvalidStoredModelsMigrateToUnknown() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::Unknown),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, rvf::kCameraProfileVersion, 0)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::Unknown),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, rvf::kCameraProfileVersion, 3)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(rvf::CameraModel::Unknown),
        static_cast<int>(rvf::cameraModelFromStoredProfile(true, rvf::kCameraProfileVersion, 255)));
}

}  // namespace

void runCameraProfileMigrationTests() {
    RUN_TEST(testCurrentProfileLoadsValidatedGr4Model);
    RUN_TEST(testCurrentProfilePreservesReservedGr3xModel);
    RUN_TEST(testMissingModelKeyMigratesToUnknown);
    RUN_TEST(testLegacyProfileVersionMigratesToUnknown);
    RUN_TEST(testInvalidStoredModelsMigrateToUnknown);
}
