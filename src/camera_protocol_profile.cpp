#include "camera_protocol_profile.h"

#include <cstring>

namespace {

CameraProtocolProfile makeGr3Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr3Family;
  profile.wifiActivationMethod = WifiActivationMethod::BleNetworkTypeUuid;
  profile.wifiCredentialMethod = WifiCredentialMethod::BleUuidCharacteristics;
  profile.requiresAuthenticatedLink = true;
  return profile;
}

CameraProtocolProfile makeGr4Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr4Family;
  profile.wifiActivationMethod = WifiActivationMethod::BleFixedHandle;
  profile.wifiCredentialMethod = WifiCredentialMethod::BleFixedHandles;
  profile.requiresAuthenticatedLink = false;
  return profile;
}

const CameraProtocolProfile kUnknownProfile;
const CameraProtocolProfile kGr3Profile = makeGr3Profile();
const CameraProtocolProfile kGr4Profile = makeGr4Profile();

}  // namespace

const CameraProtocolProfile& cameraProtocolProfile(RicohProtocolGeneration generation) {
  switch (generation) {
    case RicohProtocolGeneration::Gr3Family:
      return kGr3Profile;
    case RicohProtocolGeneration::Gr4Family:
      return kGr4Profile;
    case RicohProtocolGeneration::Unknown:
      return kUnknownProfile;
  }
  return kUnknownProfile;
}

RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence) {
  // A GR IV never exposes the GR III WLAN service. Once that service is seen,
  // a Gr4Family verdict is off the table: either the network-type
  // characteristic confirms GR III, or the evidence is contradictory (likely a
  // transient discovery failure) and no generation may be trusted with writes.
  if (evidence.hasGr3WlanService) {
    return evidence.hasGr3NetworkTypeCharacteristic ? RicohProtocolGeneration::Gr3Family
                                                    : RicohProtocolGeneration::Unknown;
  }
  if (evidence.hasGr4ControlService || evidence.gr4PowerHandleReadSucceeded) {
    return RicohProtocolGeneration::Gr4Family;
  }
  return RicohProtocolGeneration::Unknown;
}

const char* ricohProtocolGenerationName(RicohProtocolGeneration generation) {
  switch (generation) {
    case RicohProtocolGeneration::Gr3Family:
      return "GR3_FAMILY";
    case RicohProtocolGeneration::Gr4Family:
      return "GR4_FAMILY";
    case RicohProtocolGeneration::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

bool validGr3WifiChannel(uint8_t channel) {
  // 0 = camera did not report a channel (treated as "auto"); 12-14 are legal
  // in ETSI/Japan domains, so rejecting them strands non-FCC cameras.
  return channel <= 14;
}

bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel) {
  return ssid != nullptr && passphrase != nullptr &&
         std::strlen(ssid) > 0 && std::strlen(passphrase) > 0 &&
         validGr3WifiChannel(channel);
}

uint8_t parseGr3WifiChannel(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return 0;
  }

  // Prefer an ASCII decimal reading ("11", "6\0"): a binary channel byte can
  // never be all digit codes, while an ASCII channel misread as binary would
  // always fail validation (e.g. "1" = 0x31 = 49).
  unsigned value = 0;
  bool sawDigit = false;
  bool ascii = true;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = data[i];
    if (byte == 0) {
      continue;
    }
    if (byte < '0' || byte > '9' || value > 25) {
      ascii = false;
      break;
    }
    value = value * 10 + static_cast<unsigned>(byte - '0');
    sawDigit = true;
  }
  if (ascii && sawDigit) {
    return static_cast<uint8_t>(value);
  }
  return data[0];
}
