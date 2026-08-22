#include "ble_discovery_policy.h"

bool shouldRefreshProtocolDiscoveryCharacteristics(BleProtocolDiscoveryService service) {
  switch (service) {
    case BleProtocolDiscoveryService::SharedWlan:
    case BleProtocolDiscoveryService::Camera:
    case BleProtocolDiscoveryService::Shooting:
      return true;
    case BleProtocolDiscoveryService::Control:
      return false;
  }
  return false;
}
