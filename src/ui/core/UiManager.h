#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../display/DisplaySurface.h"
#include "../model/UiModel.h"

namespace rvf::ui {

namespace detail {

inline uint32_t hashByte(uint32_t hash, uint8_t value) {
    return (hash ^ value) * 16777619UL;
}

inline uint32_t hashBytes(uint32_t hash, const void* data, size_t length) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) {
        hash = hashByte(hash, bytes[i]);
    }
    return hash;
}

inline uint32_t hashText(uint32_t hash, const char* text) {
    const char* value = text != nullptr ? text : "";
    while (*value != '\0') {
        hash = hashByte(hash, static_cast<uint8_t>(*value));
        ++value;
    }
    return hashByte(hash, 0);
}

inline uint32_t modelFingerprint(const UiModel& model) {
    uint32_t hash = 2166136261UL;
    hash = hashBytes(hash, &model.screen, sizeof(model.screen));
    hash = hashBytes(hash, &model.phase, sizeof(model.phase));
    hash = hashBytes(hash, &model.appState, sizeof(model.appState));
    hash = hashBytes(hash, &model.bleConnected, sizeof(model.bleConnected));
    hash = hashBytes(hash, &model.wifiConnected, sizeof(model.wifiConnected));
    hash = hashBytes(hash, &model.previewRunning, sizeof(model.previewRunning));
    hash = hashBytes(hash, &model.cameraStandby, sizeof(model.cameraStandby));
    hash = hashBytes(hash, &model.shutterReady, sizeof(model.shutterReady));
    hash = hashBytes(hash, &model.shutterStatus, sizeof(model.shutterStatus));
    hash = hashBytes(hash, &model.wifiRssi, sizeof(model.wifiRssi));
    hash = hashBytes(hash, &model.fps, sizeof(model.fps));
    hash = hashBytes(hash, &model.renderedFrames, sizeof(model.renderedFrames));
    hash = hashBytes(hash, &model.droppedFrames, sizeof(model.droppedFrames));
    hash = hashText(hash, model.cameraName);
    hash = hashText(hash, model.cameraModel);
    hash = hashText(hash, model.battery);
    hash = hashText(hash, model.localIp);
    hash = hashText(hash, model.detail);
    hash = hashBytes(hash, &model.errorCode, sizeof(model.errorCode));
    return hashText(hash, model.errorMessage);
}

}  // namespace detail

template <typename Renderer>
class UiManager {
public:
    UiManager(display::M5DisplaySurface& surface,
              Renderer& renderer,
              uint32_t statusMinRedrawMs = 1500)
        : _surface(surface),
          _renderer(renderer),
          _statusMinRedrawMs(statusMinRedrawMs) {}

    void update(const UiModel& model, uint32_t nowMs, bool force = false) {
        // JPEG decoding owns the base pixels for LiveView. A normal status
        // update must never clear or present that surface between frames.
        if (model.screen == UiScreen::LiveView) {
            _lastScreen = model.screen;
            _dirty = false;
            return;
        }

        const uint32_t fingerprint = detail::modelFingerprint(model);
        if (fingerprint != _lastFingerprint || model.screen != _lastScreen) {
            _dirty = true;
        }

        if (!_dirty && !force) {
            return;
        }
        if (!force && _lastRenderAt != 0 &&
            (nowMs - _lastRenderAt) < _statusMinRedrawMs) {
            return;
        }

        switch (model.screen) {
            case UiScreen::Boot:
                _renderer.renderBoot(_surface.canvas(), model);
                break;
            case UiScreen::Status:
                _renderer.renderStatus(_surface.canvas(), model);
                break;
            case UiScreen::Error:
                _renderer.renderError(_surface.canvas(), model);
                break;
            case UiScreen::Shutdown:
                _renderer.renderShutdown(_surface.canvas(), model);
                break;
            case UiScreen::LiveView:
                return;
        }

        _surface.present();
        _lastFingerprint = fingerprint;
        _lastScreen = model.screen;
        _lastRenderAt = nowMs;
        _dirty = false;
    }

    void renderLiveViewOverlay(const UiModel& model) {
        _renderer.renderLiveViewOverlay(_surface.canvas(), model);
        _lastScreen = UiScreen::LiveView;
        _dirty = false;
    }

    void invalidate() {
        _dirty = true;
    }

private:
    display::M5DisplaySurface& _surface;
    Renderer& _renderer;
    uint32_t _statusMinRedrawMs = 1500;
    uint32_t _lastFingerprint = 0;
    uint32_t _lastRenderAt = 0;
    UiScreen _lastScreen = UiScreen::Boot;
    bool _dirty = true;
};

}  // namespace rvf::ui
