#pragma once

#include <cstdint>

namespace rvf {

enum class RecoveryCause : uint8_t {
    WifiDisconnected,
    BleDisconnected,
    PreviewClosed,
    PreviewReadFailed,
    FrameStalled,
    StreamStalled,
    ShutterBleNotReady,
    ShutterFailed,
    HttpOrPreviewUnavailable,
    ScheduledRetry,
    Manual,
};

inline const char* recoveryCauseName(RecoveryCause cause) {
    switch (cause) {
        case RecoveryCause::WifiDisconnected: return "WiFi disconnected";
        case RecoveryCause::BleDisconnected: return "BLE disconnected";
        case RecoveryCause::PreviewClosed: return "LiveView closed";
        case RecoveryCause::PreviewReadFailed: return "LiveView read failed";
        case RecoveryCause::FrameStalled: return "LiveView frame stall watchdog";
        case RecoveryCause::StreamStalled: return "LiveView stream stall watchdog";
        case RecoveryCause::ShutterBleNotReady: return "Button A shutter BLE not ready";
        case RecoveryCause::ShutterFailed: return "Button A BLE shutter failed";
        case RecoveryCause::HttpOrPreviewUnavailable: return "HTTP/LiveView unavailable";
        case RecoveryCause::ScheduledRetry: return "scheduled retry";
        case RecoveryCause::Manual: return "camera recovery";
    }
    return "camera recovery";
}

}  // namespace rvf
