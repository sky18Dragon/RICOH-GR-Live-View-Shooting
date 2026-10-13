#include "Gr4LegacyPairingStrategy.h"

#include "../common/RicohDiscoverySecurityBaseline.h"

// GR IV pairs with the frozen legacy baseline, unchanged.
void Gr4LegacyPairingStrategy::apply() {
  RicohDiscoverySecurityBaseline::apply();
}
