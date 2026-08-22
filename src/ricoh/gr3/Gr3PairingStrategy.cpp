#include "Gr3PairingStrategy.h"

#include <NimBLEDevice.h>

extern "C" {
#ifdef USING_NIMBLE_ARDUINO_HEADERS
#include "nimble/nimble/host/include/host/ble_gap.h"
#include "nimble/nimble/host/include/host/ble_hs.h"
#else
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#endif
}

void Gr3PairingStrategy::apply() {
  NimBLEDevice::setSecurityAuth(true, true, true);
  ble_hs_cfg.sm_keypress = 1;
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_DISPLAY);
  NimBLEDevice::setSecurityInitKey(
      BLE_SM_PAIR_KEY_DIST_ENC |
      BLE_SM_PAIR_KEY_DIST_ID |
      BLE_SM_PAIR_KEY_DIST_SIGN);
  NimBLEDevice::setSecurityRespKey(
      BLE_SM_PAIR_KEY_DIST_ENC |
      BLE_SM_PAIR_KEY_DIST_ID |
      BLE_SM_PAIR_KEY_DIST_SIGN);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
}
