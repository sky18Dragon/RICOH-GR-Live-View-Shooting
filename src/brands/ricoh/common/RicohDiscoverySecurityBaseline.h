#pragma once

// The security configuration applied before a camera is identified (read-only
// discovery) and whenever no model-specific profile applies. It is also GR IV's
// own pairing configuration, frozen byte for byte, so it lives here for every
// firmware rather than in gr4/.
class RicohDiscoverySecurityBaseline {
public:
  static void apply();
};
