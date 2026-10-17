#pragma once

#include <cstdint>

// BLE connection parameters in Bluetooth Core units:
//   interval: 1.25 ms, latency: connection events, supervision timeout: 10 ms.
// Kept free of NimBLE/Arduino headers so the native tests can include it.
struct BleConnParams {
  uint16_t minInterval = 0;
  uint16_t maxInterval = 0;
  uint16_t latency = 0;
  uint16_t supervisionTimeout = 0;
};

enum class BleLinkMode : uint8_t {
  // Connection parameters the link is opened with. Shortest shutter and GATT
  // round trips; used for pairing, credential reads and the portrait shutter.
  Normal,
  // Landscape LiveView with BLE kept up for the shutter. ESP32-S3 and the
  // camera's combo chip both time-share one 2.4 GHz radio between Wi-Fi and
  // BLE, so fewer BLE connection events leave more airtime for the MJPEG
  // stream. Costs up to one interval of extra BLE shutter latency.
  LiveViewLowDuty,
};

// Must stay byte-for-byte equal to the parameters RicohBleClient::connect()
// passes to NimBLE: BLE_GAP_INITIAL_CONN_ITVL_MIN/MAX (30/50 ms), latency 1,
// 2 * BLE_GAP_INITIAL_SUPERVISION_TIMEOUT (5.12 s). ricoh_ble_client.cpp
// static_asserts this.
constexpr uint16_t BLE_NORMAL_CONN_ITVL_MIN = 24;            // 30 ms
constexpr uint16_t BLE_NORMAL_CONN_ITVL_MAX = 40;            // 50 ms
constexpr uint16_t BLE_NORMAL_CONN_LATENCY = 1;
constexpr uint16_t BLE_NORMAL_SUPERVISION_TIMEOUT = 512;     // 5.12 s

// 82 * 1.25 ms = 102.5 ms, latency 0, 6 s supervision. Disabled by default
// (BLE_LIVEVIEW_LOW_DUTY_ENABLED in config.h): the camera is the AP, and an
// interval 0.1 ms off its 102.4 ms beacon walks BLE events across the beacon
// slowly, so they can collide with it for seconds at a time. A GR IIIx test
// lost a shutter write and hit supervision timeout in this mode. Pick an
// interval that is not near a beacon multiple before re-enabling.
constexpr uint16_t BLE_LIVEVIEW_CONN_ITVL = 82;              // 102.5 ms
constexpr uint16_t BLE_LIVEVIEW_CONN_LATENCY = 0;
constexpr uint16_t BLE_LIVEVIEW_SUPERVISION_TIMEOUT = 600;   // 6 s

inline BleConnParams bleConnParamsFor(BleLinkMode mode) {
  BleConnParams params;
  if (mode == BleLinkMode::LiveViewLowDuty) {
    params.minInterval = BLE_LIVEVIEW_CONN_ITVL;
    params.maxInterval = BLE_LIVEVIEW_CONN_ITVL;
    params.latency = BLE_LIVEVIEW_CONN_LATENCY;
    params.supervisionTimeout = BLE_LIVEVIEW_SUPERVISION_TIMEOUT;
  } else {
    params.minInterval = BLE_NORMAL_CONN_ITVL_MIN;
    params.maxInterval = BLE_NORMAL_CONN_ITVL_MAX;
    params.latency = BLE_NORMAL_CONN_LATENCY;
    params.supervisionTimeout = BLE_NORMAL_SUPERVISION_TIMEOUT;
  }
  return params;
}

// Core spec ranges, plus the rule that the supervision timeout must exceed
// (1 + latency) * maxInterval * 2.
inline bool bleConnParamsValid(const BleConnParams& params) {
  if (params.minInterval < 6 || params.maxInterval > 3200 ||
      params.minInterval > params.maxInterval) {
    return false;
  }
  if (params.latency > 499) {
    return false;
  }
  if (params.supervisionTimeout < 10 || params.supervisionTimeout > 3200) {
    return false;
  }
  const uint32_t timeoutUs = static_cast<uint32_t>(params.supervisionTimeout) * 10000U;
  const uint32_t minimumUs = (1U + params.latency) * static_cast<uint32_t>(params.maxInterval) * 1250U * 2U;
  return timeoutUs > minimumUs;
}

inline const char* bleLinkModeName(BleLinkMode mode) {
  return mode == BleLinkMode::LiveViewLowDuty ? "LIVEVIEW_LOW_DUTY" : "NORMAL";
}

struct BleLinkModeInputs {
  bool bleConnected = false;
  // Landscape or LiveView Lock: the user wants the preview.
  bool previewRequested = false;
  // MJPEG stream is open on Wi-Fi.
  bool previewStreaming = false;
  // millis() when previewStreaming last became true.
  uint32_t streamingSinceMs = 0;
  uint32_t nowMs = 0;
};

// Low duty only once the stream has run for settleMs, so LiveView open,
// recycle and recovery keep the fast link. A short stream gap while the
// preview is still wanted keeps whatever mode is in force rather than
// flapping the parameters. Leaving the preview restores Normal at once.
inline BleLinkMode desiredBleLinkMode(BleLinkMode current,
                                      const BleLinkModeInputs& in,
                                      uint32_t settleMs) {
  if (!in.bleConnected || !in.previewRequested) {
    return BleLinkMode::Normal;
  }
  if (!in.previewStreaming) {
    return current;
  }
  if (static_cast<uint32_t>(in.nowMs - in.streamingSinceMs) >= settleMs) {
    return BleLinkMode::LiveViewLowDuty;
  }
  return current;
}
