#pragma once

bool canCleanupBleScanSession(bool scanStarted,
                              bool stopRequested,
                              bool stopSucceeded,
                              bool scanEnded,
                              bool scanInactive,
                              bool callbacksQuiescent,
                              bool hostBarrierReached);

bool canClearBleStackObjects(bool hostStopSucceeded);
bool canRestartBleStack(bool hostStopSucceeded,
                        bool clearObjectsRequested,
                        bool objectClearSucceeded);
bool canUseBleStack(bool restartBlocked);
