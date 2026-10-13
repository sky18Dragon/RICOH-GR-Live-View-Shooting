#include "Gr4LegacyPairingStrategy.h"

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

void Gr4LegacyPairingStrategy::apply() {
  // GR4_LEGACY_BASELINE: keep this byte-for-byte equivalent to the
  // pre-GR-III production security configuration until hardware regression
  // testing explicitly approves a change.
  NimBLEDevice::setSecurityAuth(true, true, true);
  ble_hs_cfg.sm_keypress = 0;
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
  NimBLEDevice::setSecurityInitKey(
      BLE_SM_PAIR_KEY_DIST_ENC |
      BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(
      BLE_SM_PAIR_KEY_DIST_ENC |
      BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT);
}
