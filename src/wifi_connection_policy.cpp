#include "wifi_connection_policy.h"

namespace {

uint32_t takeBudget(uint32_t requestedMs, uint32_t& remainingMs) {
  const uint32_t granted = requestedMs < remainingMs ? requestedMs : remainingMs;
  remainingMs -= granted;
  return granted;
}

}  // namespace

size_t buildWifiConnectPlan(bool hasBssid,
                            uint8_t channel,
                            uint32_t totalTimeoutMs,
                            uint32_t bssidTimeoutMs,
                            uint32_t channelTimeoutMs,
                            bool allowFallback,
                            WifiConnectAttempt out[WIFI_CONNECT_PLAN_MAX_ATTEMPTS]) {
  if (out == nullptr || totalTimeoutMs == 0) {
    return 0;
  }

  size_t count = 0;
  uint32_t remainingMs = totalTimeoutMs;

  if (hasBssid) {
    WifiConnectAttempt& attempt = out[count++];
    attempt.target = channel != 0
                       ? WifiConnectTarget::BssidAndChannel
                       : WifiConnectTarget::BssidAnyChannel;
    attempt.useBssid = true;
    attempt.channel = channel;
    attempt.timeoutMs = allowFallback
                          ? takeBudget(bssidTimeoutMs, remainingMs)
                          : takeBudget(totalTimeoutMs, remainingMs);
  } else if (channel != 0) {
    WifiConnectAttempt& attempt = out[count++];
    attempt.target = WifiConnectTarget::ChannelOnly;
    attempt.useBssid = false;
    attempt.channel = channel;
    attempt.timeoutMs = allowFallback
                          ? takeBudget(channelTimeoutMs, remainingMs)
                          : takeBudget(totalTimeoutMs, remainingMs);
  }

  if (count == 0) {
    WifiConnectAttempt& attempt = out[count++];
    attempt.target = WifiConnectTarget::SsidOnly;
    attempt.useBssid = false;
    attempt.channel = 0;
    attempt.timeoutMs = takeBudget(totalTimeoutMs, remainingMs);
  }

  if (!allowFallback || remainingMs == 0 || count >= WIFI_CONNECT_PLAN_MAX_ATTEMPTS) {
    return count;
  }

  if (channel != 0 && hasBssid) {
    WifiConnectAttempt& attempt = out[count++];
    attempt.target = WifiConnectTarget::ChannelOnly;
    attempt.useBssid = false;
    attempt.channel = channel;
    attempt.timeoutMs = takeBudget(channelTimeoutMs, remainingMs);
  }

  if (remainingMs > 0 && count < WIFI_CONNECT_PLAN_MAX_ATTEMPTS) {
    WifiConnectAttempt& attempt = out[count++];
    attempt.target = WifiConnectTarget::SsidOnly;
    attempt.useBssid = false;
    attempt.channel = 0;
    attempt.timeoutMs = remainingMs;
    remainingMs = 0;
  }

  return count;
}

const char* wifiConnectTargetName(WifiConnectTarget target) {
  switch (target) {
    case WifiConnectTarget::BssidAndChannel:
      return "BSSID_CHANNEL";
    case WifiConnectTarget::BssidAnyChannel:
      return "BSSID_ANY_CHANNEL";
    case WifiConnectTarget::ChannelOnly:
      return "CHANNEL_ONLY";
    case WifiConnectTarget::SsidOnly:
    default:
      return "SSID_ONLY";
  }
}

bool wifiDisconnectReasonRequiresSecurityReset(int reason) {
  return reason == WIFI_DISCONNECT_REASON_INVALID_PMKID;
}

bool wifiDisconnectReasonRequiresImmediateRetry(int reason) {
  return reason == WIFI_DISCONNECT_REASON_NO_AP_FOUND;
}

bool wifiCacheNeedsPersistence(bool alreadyCached,
                               bool bssidChanged,
                               bool channelChanged) {
  return !alreadyCached || bssidChanged || channelChanged;
}
