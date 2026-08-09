#include "ble_reconnect_policy.h"

#include <cctype>
#include <cstring>

bool hasDirectBleReconnectIdentity(const char* bleAddress, bool bleAddressTypeKnown) {
  return bleAddressTypeKnown && bleAddress != nullptr && std::strlen(bleAddress) > 0;
}

bool shouldAttemptDirectBleReconnect(bool initialBootFlow,
                                     bool bonded,
                                     bool protocolKnown,
                                     const char* bleAddress,
                                     bool bleAddressTypeKnown) {
  return initialBootFlow &&
         bonded &&
         protocolKnown &&
         hasDirectBleReconnectIdentity(bleAddress, bleAddressTypeKnown);
}

bool bleCandidateMatchesStoredIdentity(const char* storedBleAddress,
                                       const char* candidateBleAddress) {
  if (storedBleAddress == nullptr || storedBleAddress[0] == '\0') {
    return true;
  }
  if (candidateBleAddress == nullptr || candidateBleAddress[0] == '\0') {
    return false;
  }

  size_t index = 0;
  while (storedBleAddress[index] != '\0' && candidateBleAddress[index] != '\0') {
    const unsigned char stored = static_cast<unsigned char>(storedBleAddress[index]);
    const unsigned char candidate = static_cast<unsigned char>(candidateBleAddress[index]);
    if (std::tolower(stored) != std::tolower(candidate)) {
      return false;
    }
    ++index;
  }
  return storedBleAddress[index] == '\0' && candidateBleAddress[index] == '\0';
}

BleBondPersistenceDecision decideBleBondPersistence(bool peerWasBonded,
                                                     bool connected,
                                                     bool bondedNow,
                                                     unsigned long elapsedMs,
                                                     unsigned long timeoutMs) {
  if (!connected) {
    return BleBondPersistenceDecision::Disconnected;
  }
  if (peerWasBonded || bondedNow) {
    return BleBondPersistenceDecision::Ready;
  }
  if (elapsedMs >= timeoutMs) {
    return BleBondPersistenceDecision::TimedOut;
  }
  return BleBondPersistenceDecision::Wait;
}
