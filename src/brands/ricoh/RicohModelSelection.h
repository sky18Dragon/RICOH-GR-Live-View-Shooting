#pragma once

// Ricoh models compiled into this firmware, chosen per build env in
// platformio.ini (-D RVF_RICOH_GR3=0 ...). Every model is in unless switched
// off, so a plain build is the full firmware.
#ifndef RVF_RICOH_GR2
#define RVF_RICOH_GR2 1
#endif
#ifndef RVF_RICOH_GR3
#define RVF_RICOH_GR3 1
#endif
#ifndef RVF_RICOH_GR4
#define RVF_RICOH_GR4 1
#endif

#if !RVF_RICOH_GR2 && !RVF_RICOH_GR3 && !RVF_RICOH_GR4
#error "Enable at least one Ricoh model (RVF_RICOH_GR2 / RVF_RICOH_GR3 / RVF_RICOH_GR4)"
#endif
