#include "Gr2Profile.h"

namespace {

CameraProtocolProfile makeGr2Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr2Family;
  // GR II is a Wi-Fi-only camera. The official GR Remote web application uses
  // /v1/liveview and the camera/shoot HTTP family at 192.168.0.1; WLAN must be
  // enabled on the body and its camera-specific WPA2 credentials configured
  // manually on the controller.
  profile.capabilities.supportsHttpLiveView = true;
  profile.capabilities.supportsHttpShutter = true;
  profile.wifiActivationMethod = WifiActivationMethod::ManualOnly;
  profile.wifiCredentialMethod = WifiCredentialMethod::ManualConfiguration;
  profile.liveViewWithoutMaxFps = true;
  profile.fixedModelLabel = "RICOH GR II";
  profile.httpShutterUsesGrRemoteSequence = true;
  profile.httpWlanFinishPowersOff = true;
  return profile;
}

}  // namespace

const CameraProtocolProfile& gr2CameraProtocolProfile() {
  static const CameraProtocolProfile profile = makeGr2Profile();
  return profile;
}
