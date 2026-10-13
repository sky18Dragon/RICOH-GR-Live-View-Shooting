#include "BleSecurityProfile.h"

#include "../RicohModelSelection.h"
#include "RicohDiscoverySecurityBaseline.h"
#if RVF_RICOH_GR3
#include "../gr3/Gr3PairingStrategy.h"
#endif
#if RVF_RICOH_GR4
#include "../gr4/Gr4LegacyPairingStrategy.h"
#endif

void applyRicohSecurityProfile(RicohSecurityProfileId profile) {
#if RVF_RICOH_GR3
  if (profile == RicohSecurityProfileId::Gr3Passkey) {
    Gr3PairingStrategy::apply();
    return;
  }
#endif
#if RVF_RICOH_GR4
  if (profile == RicohSecurityProfileId::Gr4Legacy) {
    Gr4LegacyPairingStrategy::apply();
    return;
  }
#endif
  // Discovery mode performs no security procedure. Configure the frozen
  // baseline as the inert default so an accidental security request cannot
  // silently alter behavior for the existing product population.
  RicohDiscoverySecurityBaseline::apply();
}
