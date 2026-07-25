#include "UiAnimator.h"

namespace rvf {

uint32_t uiElapsedMs(uint32_t nowMs, uint32_t startedAtMs) {
    return nowMs - startedAtMs;
}

float uiClamp01(float value) {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value;
}

float uiSmoothStep(float value) {
    const float t = uiClamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float uiEaseOut(float value) {
    const float t = uiClamp01(value);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse;
}

void AnimationState::start(uint32_t nowMs, uint32_t duration) {
    startedAtMs = nowMs;
    durationMs = duration;
    active = true;
}

void AnimationState::stop() {
    active = false;
}

float AnimationState::progress(uint32_t nowMs) const {
    if (!active || durationMs == 0) {
        return active ? 1.0f : 0.0f;
    }
    return uiClamp01(static_cast<float>(uiElapsedMs(nowMs, startedAtMs)) /
                     static_cast<float>(durationMs));
}

bool AnimationState::update(uint32_t nowMs) {
    if (!active) return false;
    if (durationMs == 0 || uiElapsedMs(nowMs, startedAtMs) >= durationMs) {
        active = false;
        return true;
    }
    return false;
}

void BacklightAnimator::begin(uint8_t brightness) {
    _from = brightness;
    _current = brightness;
    _target = brightness;
    _animation.stop();
}

void BacklightAnimator::setTarget(uint8_t brightness, uint32_t durationMs, uint32_t nowMs) {
    if (_target == brightness && (_animation.active || _current == brightness)) return;
    _from = _current;
    _target = brightness;
    if (durationMs == 0 || _from == _target) {
        _current = _target;
        _animation.stop();
        return;
    }
    _animation.start(nowMs, durationMs);
}

bool BacklightAnimator::update(uint32_t nowMs) {
    if (!_animation.active) return false;
    const uint8_t previous = _current;
    const float progress = uiSmoothStep(_animation.progress(nowMs));
    const float value = static_cast<float>(_from) +
                        (static_cast<float>(_target) - static_cast<float>(_from)) * progress;
    _current = static_cast<uint8_t>(value + 0.5f);
    if (_animation.update(nowMs)) _current = _target;
    return _current != previous;
}

}  // namespace rvf
