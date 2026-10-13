#pragma once

#include <cstdint>

// Handle mapping and AP value were verified from RICOH GR Android app BLE
// traffic captured on 2026-06-27. OFF follows the Network Type characteristic
// value mapping at the same strictly detected GR IV handle; keep write-with-
// response and validate the transition on GR IV hardware.
constexpr uint16_t RICOH_BLE_GR4_WLAN_POWER_HANDLE = 0x0135;
constexpr uint8_t RICOH_BLE_GR4_WLAN_ON_VALUE = 0x01;
constexpr uint8_t RICOH_BLE_GR4_WLAN_OFF_VALUE = 0x00;
constexpr uint16_t RICOH_BLE_GR4_WLAN_SSID_HANDLE = 0x0138;
constexpr uint16_t RICOH_BLE_GR4_WLAN_PASSPHRASE_HANDLE = 0x013A;
constexpr uint16_t RICOH_BLE_GR4_WLAN_SECURITY_HANDLE = 0x013C;
constexpr uint16_t RICOH_BLE_GR4_WLAN_FREQUENCY_HANDLE = 0x013E;
constexpr uint16_t RICOH_BLE_GR4_WLAN_BSSID_HANDLE = 0x0140;

// Verified from Android HCI logs captured on 2026-06-28.
// 0x01: camera powered on / controllable, 0x00: power-off or shutting down.
constexpr uint16_t RICOH_BLE_GR4_POWER_STATE_HANDLE = 0x00EB;
constexpr uint16_t RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE = 0x00EC;
constexpr uint8_t RICOH_BLE_GR4_POWER_STATE_ON_VALUE = 0x01;
constexpr uint8_t RICOH_BLE_GR4_POWER_STATE_OFF_VALUE = 0x00;
