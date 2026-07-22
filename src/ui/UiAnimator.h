#pragma once

#include <cstdint>

namespace rvf {

uint32_t uiElapsedMs(uint32_t nowMs, uint32_t startedAtMs);
float uiClamp01(float value);
float uiSmoothStep(float value);
float uiEaseOut(float value);

struct AnimationState {
    uint32_t startedAtMs = 0;
    uint32_t durationMs = 0;
    bool active = false;

    void start(uint32_t nowMs, uint32_t duration);
    void stop();
    float progress(uint32_t nowMs) const;
    bool update(uint32_t nowMs);
};

class BacklightAnimator {
public:
    void begin(uint8_t brightness);
    void setTarget(uint8_t brightness, uint32_t durationMs, uint32_t nowMs);
    bool update(uint32_t nowMs);
    uint8_t value() const { return _current; }
    uint8_t target() const { return _target; }

private:
    uint8_t _from = 0;
    uint8_t _current = 0;
    uint8_t _target = 0;
    AnimationState _animation;
};

}  // namespace rvf
