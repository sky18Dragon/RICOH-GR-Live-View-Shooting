#pragma once

#include <cstdint>
#include <string>

// Camera access-point credentials read over BLE, accumulated across polls.
struct RicohWifiCredentials {
  bool valid = false;
  bool encryptedPassphrase = false;
  int securityType = -1;
  uint16_t frequencyMhz = 0;
  uint8_t channel = 0;
  std::string ssid;
  std::string passphrase;
  std::string bssid;
};
