#include "ble_scan_lifecycle_policy.h"

bool canCleanupBleScanSession(bool scanStarted,
                              bool stopRequested,
                              bool stopSucceeded,
                              bool scanEnded,
                              bool scanInactive,
                              bool callbacksQuiescent,
                              bool hostBarrierReached) {
  if (!callbacksQuiescent) {
    return false;
  }
  if (!scanStarted) {
    return true;
  }
  if (!scanInactive) {
    return false;
  }
  // NimBLE-Arduino 2.5.0 does not emit onScanEnd() from stop().  An explicit
  // successful cancel plus an inactive controller is therefore the terminal
  // state; natural timeout/completion still requires onScanEnd().
  return stopRequested
           ? stopSucceeded && hostBarrierReached
           : scanEnded;
}

bool canClearBleStackObjects(bool hostStopSucceeded) {
  return hostStopSucceeded;
}

bool canRestartBleStack(bool hostStopSucceeded,
                        bool clearObjectsRequested,
                        bool objectClearSucceeded) {
  return hostStopSucceeded &&
         (!clearObjectsRequested || objectClearSucceeded);
}

bool canUseBleStack(bool restartBlocked) {
  return !restartBlocked;
}
