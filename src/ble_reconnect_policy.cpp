#include "ble_reconnect_policy.h"

#include <cstring>

bool hasDirectBleReconnectIdentity(const char* bleAddress, bool bleAddressTypeKnown) {
  return bleAddressTypeKnown && bleAddress != nullptr && std::strlen(bleAddress) > 0;
}
