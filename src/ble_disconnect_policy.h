#pragma once

#include <cstdint>

enum class RicohBleDisconnectKind : uint8_t {
  None,
  SupervisionTimeout,
  RemoteUser,
  RemotePowerOff,
  LocalHost,
  ConnectionEstablishmentFailed,
  Other,
};

// NimBLE wraps Bluetooth HCI status values as 0x200 + HCI status.
constexpr int RICOH_NIMBLE_REASON_SUPERVISION_TIMEOUT = 0x208;
constexpr int RICOH_NIMBLE_REASON_REMOTE_USER = 0x213;
constexpr int RICOH_NIMBLE_REASON_REMOTE_POWER_OFF = 0x215;
constexpr int RICOH_NIMBLE_REASON_LOCAL_HOST = 0x216;
constexpr int RICOH_NIMBLE_REASON_ESTABLISHMENT_FAILED = 0x23E;

inline RicohBleDisconnectKind classifyRicohBleDisconnect(int reason) {
  switch (reason) {
    case 0:
      return RicohBleDisconnectKind::None;
    case RICOH_NIMBLE_REASON_SUPERVISION_TIMEOUT:
      return RicohBleDisconnectKind::SupervisionTimeout;
    case RICOH_NIMBLE_REASON_REMOTE_USER:
      return RicohBleDisconnectKind::RemoteUser;
    case RICOH_NIMBLE_REASON_REMOTE_POWER_OFF:
      return RicohBleDisconnectKind::RemotePowerOff;
    case RICOH_NIMBLE_REASON_LOCAL_HOST:
      return RicohBleDisconnectKind::LocalHost;
    case RICOH_NIMBLE_REASON_ESTABLISHMENT_FAILED:
      return RicohBleDisconnectKind::ConnectionEstablishmentFailed;
    default:
      return RicohBleDisconnectKind::Other;
  }
}

inline bool ricohBleDisconnectMayIndicateCameraSleep(int reason) {
  const RicohBleDisconnectKind kind = classifyRicohBleDisconnect(reason);
  // Preserve the established remote-disconnect policy while keeping local
  // teardown and ordinary transport failures out of CameraSleep.
  return kind == RicohBleDisconnectKind::RemoteUser ||
         kind == RicohBleDisconnectKind::RemotePowerOff;
}

inline const char* ricohBleDisconnectReasonName(int reason) {
  switch (classifyRicohBleDisconnect(reason)) {
    case RicohBleDisconnectKind::None:
      return "NONE";
    case RicohBleDisconnectKind::SupervisionTimeout:
      return "SUPERVISION_TIMEOUT";
    case RicohBleDisconnectKind::RemoteUser:
      return "REMOTE_USER_OR_PAIRING_REJECTED";
    case RicohBleDisconnectKind::RemotePowerOff:
      return "REMOTE_POWER_OFF";
    case RicohBleDisconnectKind::LocalHost:
      return "LOCAL_HOST_TERMINATED";
    case RicohBleDisconnectKind::ConnectionEstablishmentFailed:
      return "CONNECTION_ESTABLISHMENT_FAILED";
    case RicohBleDisconnectKind::Other:
      return "UNCLASSIFIED";
  }
  return "UNCLASSIFIED";
}
