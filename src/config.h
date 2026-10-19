#pragma once

#include <Arduino.h>

#ifndef GR2_PROVISIONING_AP_PREFIX
#define GR2_PROVISIONING_AP_PREFIX "GR-II-Setup-"
#endif

#ifndef GR2_PROVISIONING_AP_PASSWORD
#define GR2_PROVISIONING_AP_PASSWORD "GR288888"
#endif

#ifndef GR2_PROVISIONING_URL_HOST
#define GR2_PROVISIONING_URL_HOST "192.168.4.1"
#endif

#ifndef GR_HOST
#define GR_HOST "192.168.0.1"
#endif

#ifndef GR_PORT
#define GR_PORT 80
#endif

constexpr uint16_t DISPLAY_WIDTH = 240;
constexpr uint16_t DISPLAY_HEIGHT = 135;

constexpr size_t FRAME_BUFFER_SIZE = 256 * 1024;
constexpr size_t STREAM_READ_BUFFER_SIZE = 8192;

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_CHANNEL_HINT_CONNECT_TIMEOUT_MS = 6000;
// Channel-hinted join for cameras whose AP channel changes on every WLAN ON
// (wifiChannelChangesOnRestart). A GR IV on the right channel joined within
// 2.5 s; on the wrong one the 6 s hint was pure waiting before the full scan.
constexpr uint32_t WIFI_UNSTABLE_CHANNEL_HINT_CONNECT_TIMEOUT_MS = 3000;
// Start association as soon as the BLE WLAN-on write is acknowledged. The
// previous 700 ms fixed sleep plus 1200 ms attempt is preserved as one 1900 ms
// bounded attempt, allowing DHCP to complete as soon as the AP is actually up.
constexpr uint32_t WIFI_CACHED_CONNECT_TIMEOUT_MS = 1900;
constexpr uint32_t WIFI_CACHED_CONNECT_GRACE_MS = 0;
constexpr uint32_t WIFI_CACHE_REFRESH_DELAY_MS = 5000;
constexpr uint32_t BLE_SCAN_RETRY_INTERVAL_MS = 1000;
constexpr uint32_t PROPS_TIMEOUT_MS = 3500;
// AF-then-shoot can outlast a props read. Matches the curl --max-time used
// against GR IIIx /v1/camera/shoot with body af=on.
constexpr uint32_t HTTP_SHOOT_TIMEOUT_MS = 15000;
constexpr uint32_t WLAN_FINISH_TIMEOUT_MS = 1000;
constexpr uint32_t LIVEVIEW_STALL_TIMEOUT_MS = 5000;
// LiveView keeps BLE up for the shutter. When enabled, once the stream has run
// BLE_LIVEVIEW_LOW_DUTY_SETTLE_MS the BLE link drops to the low-duty
// parameters in ble_link_mode_policy.h (102.5 ms, latency 0). Off by default:
// on a GR IIIx (2026-10-13) low duty averaged ~4.0 FPS and ended in a lost
// shutter write + supervision timeout, while the connect-time 30-50 ms /
// latency 1 link averaged ~6.2 FPS with every shot confirmed. Latency 1
// already lets the camera answer only every other event, so its radio gained
// little, and 102.5 ms sits 0.1 ms off the camera's own 102.4 ms beacon.
#define BLE_LIVEVIEW_LOW_DUTY_ENABLED 0
constexpr uint32_t BLE_LIVEVIEW_LOW_DUTY_SETTLE_MS = 1500;
// Joining the camera AP right after BLE WLAN ON: the first attempt starts
// before the AP beacons, sits ~3.4 s until NO_AP_FOUND, and the retry then
// joins in ~0.6 s (GR IIIx, 2026-10-13). Re-issue the join when the STA has
// not associated after this long instead of waiting out the doomed attempt.
// A join that has associated (auth/DHCP in progress) is never interrupted.
constexpr uint32_t WIFI_CONNECT_KICK_MS = 1500;
// A full-channel scan (channel 0) plus join takes ~3.4 s; re-issuing the join
// every 1.5 s restarted the scan before it finished, so full-scan joins only
// succeeded once the kicks ran out (GR IV, 2026-10-13: 8.1 s instead of ~3.4 s).
constexpr uint32_t WIFI_CONNECT_FULL_SCAN_KICK_MS = 4000;
constexpr uint8_t WIFI_CONNECT_MAX_KICKS = 3;
// A kick is only worth it with at least this much of the timeout left.
constexpr uint32_t WIFI_CONNECT_KICK_MIN_REMAINING_MS = 1000;
constexpr uint32_t BLE_LINK_MODE_RETRY_MS = 2000;
// LINK: log line while previewing (RSSI average/min, Wi-Fi disconnects, BLE
// negotiated parameters) for coexistence and antenna-placement testing.
constexpr uint32_t LINK_STATS_LOG_INTERVAL_MS = 5000;
constexpr uint32_t LINK_RSSI_SAMPLE_INTERVAL_MS = 1000;
// The camera defaults to maxfps=90. Unthrottled, a GR IIIx's shared
// CYW43455 Wi-Fi/BT radio starved the stream to <1 FPS and dropped BLE with
// supervision timeouts; 7 held 4-7 FPS with working BLE shutter on GR IIIx
// and GR IV. GR II has not been tested with the parameter and keeps 0.
constexpr uint8_t LIVEVIEW_MAX_FPS = 7;
// GR III/IV stream 720x480 when /v1/liveview carries no reso=. A GR IIIx on
// V1.50 locked up (power LED off, battery pull needed) one second after a
// landscape BLE shot with reso=720x480 on the stream, and its resoList holds
// nothing smaller anyway, so reso= stays off until a model proves both a
// smaller size and stability with it. Even when enabled, only sizes below the
// default are ever requested.
constexpr bool LIVEVIEW_REQUEST_RESO = false;
constexpr uint16_t LIVEVIEW_DEFAULT_WIDTH = 720;
constexpr uint16_t LIVEVIEW_DEFAULT_HEIGHT = 480;
// Window after an acknowledged BLE shutter write in which a ShotCount or
// CaptureStatus notification counts as the camera confirming the capture.
// Expiry is only reported, never treated as failure: long exposures and
// cameras that never send these notifications must keep working.
constexpr uint32_t SHOT_CONFIRM_TIMEOUT_MS = 4000;
constexpr uint32_t UI_STATUS_INTERVAL_MS = 1000;
constexpr uint32_t POWER_BUTTON_POLL_MS = 50;
constexpr uint32_t POWER_BUTTON_HOLD_MS = 1200;
constexpr uint32_t POWER_BUTTON_RELEASE_WAIT_MS = 3000;
constexpr uint8_t KEY2_FALLBACK_GPIO = 12;
constexpr uint32_t KEY2_PAIRING_RESET_HOLD_MS = 3000;
constexpr uint32_t PROPS_REFRESH_INTERVAL_MS = 60000;
constexpr uint32_t INITIAL_PROPS_REFRESH_DELAY_MS = 1500;

// GR III/IIIx LiveView is 720x480. Espressif's scaler requires dimensions
// divisible by eight; 216x144 preserves 3:2 and is the first exact-aspect size
// above the 240x135 display's 203x135 contain rectangle.
constexpr uint16_t JPEG_DECODE_WIDTH = 216;
constexpr uint16_t JPEG_DECODE_HEIGHT = 144;

constexpr uint32_t BLE_SCAN_SECONDS = 2;
constexpr bool BLE_DIRECT_RECONNECT_ON_BOOT = true;
// A StickS3 hard reset leaves the camera holding the previous link until the
// negotiated 5.12 s supervision timeout expires. Keep the direct-connect
// attempt alive across that window so it can complete as soon as advertising
// resumes instead of paying for a failed connect, scan and stack reset.
constexpr uint32_t BLE_FAST_CONNECT_TIMEOUT_MS = 6000;
constexpr uint32_t BLE_CONNECT_TIMEOUT_MS = 8000;
constexpr uint8_t BLE_CONNECT_ATTEMPTS = 12;
constexpr uint32_t BLE_CONNECT_RETRY_DELAY_MS = 1000;
constexpr uint32_t BLE_FIRST_PAIRING_RETRY_DELAY_MS = 150;
constexpr uint32_t BLE_SCAN_TO_CONNECT_DELAY_MS = 50;
constexpr uint8_t BLE_STACK_RESET_AFTER_FAILURES = 2;
constexpr uint32_t BLE_STACK_RESET_DELAY_MS = 1500;
constexpr uint32_t BLE_RECOVERY_STACK_RESET_GRACE_MS = 700;
constexpr uint32_t BLE_DISCONNECT_WAIT_MS = 1200;
constexpr uint32_t RICOH_BLE_BONDED_SECURITY_WAIT_MS = 1500;
constexpr uint32_t RICOH_BLE_SECURITY_WAIT_MS = 7000;
// GR IV reports encrypted/authenticated slightly before NimBLE persists the
// new bond. A new peer must finish bond persistence inside this window before
// the connection is accepted and its profile can be saved.
constexpr uint32_t RICOH_BLE_BOND_PERSIST_SETTLE_MS = 1000;
// On-screen passkey entry window. A GR IIIx dropped the link with 0x213 about
// 30 s after asking for the passkey (2026-10-13), so a longer window only lets
// the user keep typing into a pairing the camera has already abandoned. The
// screen shows the countdown.
constexpr uint32_t RICOH_BLE_PASSKEY_ENTRY_WAIT_MS = 28000;
// After the passkey is submitted, time allowed for encryption + bonding to
// finish (~4.5 s observed on a GR IIIx). Extends the security wait so a code
// entered near the end of the window is not cut off mid-bond.
constexpr uint32_t RICOH_BLE_PASSKEY_BOND_WAIT_MS = 10000;
constexpr uint32_t PASSKEY_DIGIT_CONFIRM_HOLD_MS = 600;
constexpr uint8_t FIRST_BOOT_BLE_PAIRING_ATTEMPTS = 12;
#ifndef RVF_SERIAL_BOOT_WAIT_MS
#define RVF_SERIAL_BOOT_WAIT_MS 0
#endif
constexpr uint32_t SERIAL_BOOT_WAIT_MS = RVF_SERIAL_BOOT_WAIT_MS;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr const char* FIRMWARE_BANNER = "RICOH GR StickS3 Remote Viewfinder V2";
constexpr const char* FIRMWARE_BOOT_MESSAGE = "V2 booting...";
constexpr uint32_t CAMERA_POWER_OFF_COOLDOWN_MS = 15000;
constexpr uint32_t CAMERA_POWER_OFF_PROBE_BACKOFF_MS = 8000;
constexpr uint32_t CAMERA_SLEEP_AUTO_POWER_OFF_MS = 30000;
// resetStack() already waits BLE_STACK_RESET_DELAY_MS and synchronously
// reinitializes NimBLE. Only a short scheduler/controller settle is needed.
constexpr uint32_t BLE_MANUAL_WAKE_REINIT_SETTLE_MS = 200;
constexpr int RICOH_BLE_DISCONNECT_REMOTE_USER = 0x213;
constexpr int RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF = 0x215;
constexpr uint8_t RICOH_BLE_POWER_READ_RETRIES = 2;
constexpr uint8_t RICOH_BLE_OPERATION_MODE_READ_RETRIES = 2;
constexpr uint32_t RICOH_BLE_POWER_NOTIFY_SETTLE_MS = 30;
constexpr bool RICOH_BLE_REQUIRE_POWER_ON_BEFORE_WIFI = true;
constexpr bool RICOH_BLE_ALLOW_WIFI_WHEN_POWER_UNKNOWN = false;
constexpr bool RICOH_BLE_BLOCK_WIFI_IN_STANDBY_OPERATION_MODE = true;
// GR IIIx V1.50 can report Power=ON with OperationMode=bleStartup right after
// pairing. Keep the link, re-read the mode, then probe NetworkType once; the
// camera's bled forwards that write without a mode check and the camera
// answers with a GATT error if it refuses.
constexpr uint32_t RICOH_BLE_GR3_STARTUP_MODE_WAIT_MS = 10000;
constexpr uint32_t RICOH_BLE_GR3_STARTUP_MODE_POLL_MS = 1000;
// In bleStartup the camera rejects Shutter writes (CaptureCommand::Release
// fails, ATT 0x80). bled forwards an OperationMode write to the camera's
// SetOperationMode; only request capture when OperationModeList offers it.
constexpr bool RICOH_BLE_GR3_REQUEST_CAPTURE_IN_STARTUP_MODE = true;

constexpr bool RICOH_BLE_AUTO_WLAN_ON_BOOT = true;
constexpr uint32_t RICOH_BLE_POST_WLAN_ON_WAIT_MS = 0;
constexpr uint32_t RICOH_BLE_WIFI_CREDENTIAL_WAIT_MS = 10000;
constexpr uint32_t RICOH_BLE_WIFI_CREDENTIAL_POLL_MS = 500;
constexpr uint8_t WIFI_OPEN_ATTEMPTS = 3;

#include "brands/ricoh/common/RicohBleUuids.h"
#include "brands/ricoh/gr3/Gr3BleConfig.h"
#include "brands/ricoh/gr4/Gr4BleConfig.h"

#ifndef RICOH_BLE_GATT_DIAGNOSTICS
#define RICOH_BLE_GATT_DIAGNOSTICS 0
#endif
