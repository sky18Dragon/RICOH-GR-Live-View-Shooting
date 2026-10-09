#pragma once

#include <cstddef>
#include <cstdint>

enum class WifiConnectTarget : uint8_t {
  BssidAndChannel,
  BssidAnyChannel,
  ChannelOnly,
  SsidOnly,
};

struct WifiConnectAttempt {
  WifiConnectTarget target = WifiConnectTarget::SsidOnly;
  bool useBssid = false;
  uint8_t channel = 0;
  uint32_t timeoutMs = 0;
};

constexpr size_t WIFI_CONNECT_PLAN_MAX_ATTEMPTS = 3;
constexpr int WIFI_DISCONNECT_REASON_INVALID_PMKID = 49;
constexpr int WIFI_DISCONNECT_REASON_NO_AP_FOUND = 201;
constexpr uint8_t WIFI_NO_AP_RETRIES_WITHIN_BUDGET = 2;

size_t buildWifiConnectPlan(bool hasBssid,
                            uint8_t channel,
                            uint32_t totalTimeoutMs,
                            uint32_t bssidTimeoutMs,
                            uint32_t channelTimeoutMs,
                            bool allowFallback,
                            WifiConnectAttempt out[WIFI_CONNECT_PLAN_MAX_ATTEMPTS]);

const char* wifiConnectTargetName(WifiConnectTarget target);

bool wifiDisconnectReasonRequiresSecurityReset(int reason);
bool wifiDisconnectReasonRequiresImmediateRetry(int reason);
bool wifiCacheNeedsPersistence(bool alreadyCached,
                               bool bssidChanged,
                               bool channelChanged);
