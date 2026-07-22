#include "OrientationTracker.h"

#include <cmath>

#include "UiAnimator.h"
#include "UiTheme.h"

namespace rvf {

OrientationTracker::OrientationTracker(UiOrientation initial)
    : _orientation(initial), _candidate(initial) {}

void OrientationTracker::reset(UiOrientation orientation, uint32_t nowMs) {
    _initialized = false;
    _candidateActive = false;
    _orientation = orientation;
    _candidate = orientation;
    _filteredX = 0.0f;
    _filteredY = 0.0f;
    _filteredZ = 0.0f;
    _candidateSinceMs = nowMs;
    _lastCommittedAtMs = nowMs;
}

UiOrientation OrientationTracker::classify(float x, float y) const {
    const float absX = std::fabs(x);
    const float absY = std::fabs(y);
    if (absX < UiTheme::kOrientationMinAxisG && absY < UiTheme::kOrientationMinAxisG) {
        return _orientation;
    }
    if (absX > absY + UiTheme::kOrientationHysteresisG) {
        // StickS3 is physically portrait when gravity is aligned with its X axis.
        return UiOrientation::Portrait;
    }
    if (absY > absX + UiTheme::kOrientationHysteresisG) {
        return UiOrientation::Landscape;
    }
    return _orientation;
}

bool OrientationTracker::update(float accelX, float accelY, float accelZ, uint32_t nowMs) {
    if (!_available) return false;

    if (!_initialized) {
        _filteredX = accelX;
        _filteredY = accelY;
        _filteredZ = accelZ;
        _initialized = true;
    } else {
        const float alpha = UiTheme::kOrientationLowPassAlpha;
        _filteredX += alpha * (accelX - _filteredX);
        _filteredY += alpha * (accelY - _filteredY);
        _filteredZ += alpha * (accelZ - _filteredZ);
    }

    const UiOrientation nextCandidate = classify(_filteredX, _filteredY);
    if (nextCandidate == _orientation) {
        _candidate = _orientation;
        _candidateActive = false;
        return false;
    }

    if (!_candidateActive || nextCandidate != _candidate) {
        _candidate = nextCandidate;
        _candidateSinceMs = nowMs;
        _candidateActive = true;
        return false;
    }

    if (uiElapsedMs(nowMs, _candidateSinceMs) < UiTheme::kOrientationStableMs ||
        uiElapsedMs(nowMs, _lastCommittedAtMs) < UiTheme::kOrientationMinHoldMs) {
        return false;
    }

    _orientation = _candidate;
    _lastCommittedAtMs = nowMs;
    _candidateActive = false;
    return true;
}

}  // namespace rvf
