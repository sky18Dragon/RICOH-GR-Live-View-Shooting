#include "BleSecurityProfile.h"

#include "gr3/Gr3PairingStrategy.h"
#include "gr4/Gr4LegacyPairingStrategy.h"

void applyRicohSecurityProfile(RicohSecurityProfileId profile) {
  if (profile == RicohSecurityProfileId::Gr3Passkey) {
    Gr3PairingStrategy::apply();
    return;
  }

  // Discovery mode performs no security procedure. Configure the frozen GR IV
  // baseline as the inert default so an accidental security request cannot
  // silently alter behavior for the existing product population.
  Gr4LegacyPairingStrategy::apply();
}
