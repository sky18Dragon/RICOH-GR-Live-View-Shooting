#pragma once

// GR II pieces the Ricoh workflow calls: the Wi-Fi provisioning portal and
// the HTTP shutter. A firmware built without GR II (RVF_RICOH_GR2=0) never
// lists or selects that model, so it gets inert stand-ins instead and links
// no web server.

#include "RicohModelSelection.h"

#if RVF_RICOH_GR2

#include "gr2/Gr2HttpShutter.h"
#include "gr2/Gr2ProvisioningPortal.h"

#else

#include <Arduino.h>

class GrApi;

struct Gr2ProvisionedWifi {
  String ssid;
  String passphrase;
};

class Gr2ProvisioningPortal {
public:
  bool begin() { return false; }
  void loop() {}
  void stop() {}
  bool active() const { return false; }
  bool takeCredentials(Gr2ProvisionedWifi&) { return false; }
  const String& accessPointSsid() const { return _empty; }
  const String& accessPointPassword() const { return _empty; }
  String accessPointIp() const { return String(); }
  const String& lastError() const { return _empty; }

private:
  String _empty;
};

inline bool shootGr2(GrApi&, uint32_t) { return false; }

#endif
