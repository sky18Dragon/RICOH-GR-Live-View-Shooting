#pragma once

enum class BleProtocolDiscoveryService {
  SharedWlan,
  Camera,
  Shooting,
  Control,
};

bool shouldRefreshProtocolDiscoveryCharacteristics(BleProtocolDiscoveryService service);
