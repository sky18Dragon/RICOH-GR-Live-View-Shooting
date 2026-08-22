#pragma once

bool hasDirectBleReconnectIdentity(const char* bleAddress, bool bleAddressTypeKnown);
bool shouldAttemptDirectBleReconnect(bool initialBootFlow,
                                     bool bonded,
                                     bool protocolKnown,
                                     const char* bleAddress,
                                     bool bleAddressTypeKnown);

// With no stored identity discovery remains open. Once a camera is stored,
// reconnect must stay pinned to that address until the user clears pairing.
bool bleCandidateMatchesStoredIdentity(const char* storedBleAddress,
                                       const char* candidateBleAddress);

enum class BleBondPersistenceDecision {
  Ready,
  Wait,
  Disconnected,
  TimedOut,
};

BleBondPersistenceDecision decideBleBondPersistence(bool peerWasBonded,
                                                     bool connected,
                                                     bool bondedNow,
                                                     unsigned long elapsedMs,
                                                     unsigned long timeoutMs);
