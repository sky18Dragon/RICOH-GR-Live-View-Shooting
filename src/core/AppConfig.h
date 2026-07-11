#pragma once

#include <Arduino.h>

#include "../config.h"

namespace rvf {

struct AppConfig {
    static constexpr const char* kProjectName = "RICOH Viewfinder";
    static constexpr const char* kFirmwareRole = "StickS3 GR viewfinder";

    struct SerialPort {
        static constexpr uint32_t kBaud = 115200;
        static constexpr uint32_t kBootWaitMs = SERIAL_BOOT_WAIT_MS;
    };

    struct Ui {
        static constexpr const char* kBootMessage = "V2 booting...";
        static constexpr const char* kBanner = "RICOH GR StickS3 Remote Viewfinder V2";
        static constexpr uint16_t kDisplayWidth = DISPLAY_WIDTH;
        static constexpr uint16_t kDisplayHeight = DISPLAY_HEIGHT;
        static constexpr uint32_t kStatusIntervalMs = UI_STATUS_INTERVAL_MS;
    };

    struct Buffer {
        static constexpr size_t kFrameBufferSize = FRAME_BUFFER_SIZE;
        static constexpr size_t kStreamReadBufferSize = STREAM_READ_BUFFER_SIZE;
    };

    struct CameraHttp {
        static constexpr const char* kHost = GR_HOST;
        static constexpr uint16_t kPort = GR_PORT;
        static constexpr uint32_t kPropsTimeoutMs = PROPS_TIMEOUT_MS;
        static constexpr uint32_t kLiveViewStallTimeoutMs = LIVEVIEW_STALL_TIMEOUT_MS;
        static constexpr uint32_t kPropsRefreshIntervalMs = PROPS_REFRESH_INTERVAL_MS;
    };

    struct Wifi {
        static constexpr uint32_t kConnectTimeoutMs = WIFI_CONNECT_TIMEOUT_MS;
        static constexpr uint32_t kChannelHintConnectTimeoutMs = WIFI_CHANNEL_HINT_CONNECT_TIMEOUT_MS;
        static constexpr uint32_t kCachedConnectTimeoutMs = WIFI_CACHED_CONNECT_TIMEOUT_MS;
        static constexpr uint32_t kCachedConnectGraceMs = WIFI_CACHED_CONNECT_GRACE_MS;
        static constexpr uint32_t kCacheRefreshDelayMs = WIFI_CACHE_REFRESH_DELAY_MS;
        static constexpr uint8_t kOpenAttempts = WIFI_OPEN_ATTEMPTS;
    };

    struct Ble {
        static constexpr uint32_t kScanRetryIntervalMs = BLE_SCAN_RETRY_INTERVAL_MS;
        static constexpr uint32_t kScanSeconds = BLE_SCAN_SECONDS;
        static constexpr uint32_t kFastConnectTimeoutMs = BLE_FAST_CONNECT_TIMEOUT_MS;
        static constexpr uint32_t kConnectTimeoutMs = BLE_CONNECT_TIMEOUT_MS;
        static constexpr uint8_t kConnectAttempts = BLE_CONNECT_ATTEMPTS;
        static constexpr uint32_t kConnectRetryDelayMs = BLE_CONNECT_RETRY_DELAY_MS;
        static constexpr uint32_t kScanToConnectDelayMs = BLE_SCAN_TO_CONNECT_DELAY_MS;
        static constexpr uint8_t kStackResetAfterFailures = BLE_STACK_RESET_AFTER_FAILURES;
        static constexpr uint32_t kStackResetDelayMs = BLE_STACK_RESET_DELAY_MS;
        static constexpr uint32_t kDisconnectWaitMs = BLE_DISCONNECT_WAIT_MS;
    };

    struct RicohPower {
        static constexpr uint16_t kPowerStateHandle = RICOH_BLE_POWER_STATE_HANDLE;
        static constexpr uint16_t kPowerStateCccdHandle = RICOH_BLE_POWER_STATE_CCCD_HANDLE;
        static constexpr uint8_t kPowerStateOnValue = RICOH_BLE_POWER_STATE_ON_VALUE;
        static constexpr uint8_t kPowerStateOffValue = RICOH_BLE_POWER_STATE_OFF_VALUE;
        static constexpr uint8_t kPowerReadRetries = RICOH_BLE_POWER_READ_RETRIES;
        static constexpr bool kRequirePowerOnBeforeWifi = RICOH_BLE_REQUIRE_POWER_ON_BEFORE_WIFI;
        static constexpr bool kAllowWifiWhenPowerUnknown = RICOH_BLE_ALLOW_WIFI_WHEN_POWER_UNKNOWN;
    };
};

}  // namespace rvf
