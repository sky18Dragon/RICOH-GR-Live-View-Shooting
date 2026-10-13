#pragma once

#include "../../../camera_protocol_profile.h"

const CameraProtocolProfile& gr3CameraProtocolProfile();
const RicohSecurityProfile& gr3SecurityProfile();

// GR III family Wi-Fi parameters: SSID and passphrase present, channel 0-11.
bool validGr3WifiChannel(uint8_t channel);
bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel);
