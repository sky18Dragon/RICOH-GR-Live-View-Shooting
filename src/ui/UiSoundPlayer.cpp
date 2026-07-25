#include "UiSoundPlayer.h"

#include "UiAnimator.h"
#include "UiTheme.h"

namespace rvf {

bool UiSoundPlayer::begin() {
    if (!UiTheme::kSoundEnabled || !M5.Speaker.isEnabled()) {
        _available = false;
        return false;
    }
    _available = M5.Speaker.isRunning() || M5.Speaker.begin();
    if (_available) M5.Speaker.setVolume(UiTheme::kSoundVolume);
    return _available;
}

void UiSoundPlayer::play(UiSound sound, uint32_t nowMs) {
    if (!_available || sound == UiSound::None) return;
    if (sound == _lastSound && uiElapsedMs(nowMs, _lastSoundAtMs) < 80U) return;

    _lastSound = sound;
    _lastSoundAtMs = nowMs;
    _secondTonePending = false;
    switch (sound) {
        case UiSound::FocusTick:
            M5.Speaker.tone(880.0f, 22, 0, true);
            break;
        case UiSound::ShutterClick:
            M5.Speaker.tone(1500.0f, 22, 0, true);
            _secondTonePending = true;
            _secondToneAtMs = nowMs + 26U;
            break;
        case UiSound::Success:
            M5.Speaker.tone(1100.0f, 36, 0, true);
            break;
        case UiSound::Error:
            M5.Speaker.tone(260.0f, 65, 0, true);
            break;
        case UiSound::None:
            break;
    }
}

void UiSoundPlayer::update(uint32_t nowMs) {
    if (!_available || !_secondTonePending) return;
    if (static_cast<int32_t>(nowMs - _secondToneAtMs) < 0) return;
    _secondTonePending = false;
    M5.Speaker.tone(760.0f, 28, 0, true);
}

}  // namespace rvf
