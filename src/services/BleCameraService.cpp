#include "BleCameraService.h"

#include "../core/Logger.h"

namespace rvf {

void BleCameraService::attach(RicohBleClient& client) {
    _client = &client;
}

bool BleCameraService::attached() const {
    return _client != nullptr;
}

Result BleCameraService::begin() {
    Result ready = requireClient("begin");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "begin");
        return ready;
    }
    _client->begin();
    LOGLINE_I("BLE", "BLE service initialized");
    return Result::success();
}

Result BleCameraService::begin(RicohBleClient& client) {
    attach(client);
    return begin();
}

Result BleCameraService::scan() {
    Result ready = requireClient("scan");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "scan");
        return ready;
    }
    publish(AppEventType::BleScanStarted, 0, "scan");
    return Result::success();
}

bool BleCameraService::consumeEvent(AppMessage& message) {
    if (!_hasPendingEvent) {
        message = AppMessage{};
        return false;
    }
    message = _pendingEvent;
    _pendingEvent = AppMessage{};
    _hasPendingEvent = false;
    return true;
}

RicohBleDeviceInfo BleCameraService::scanCamera(const String& preferredAddress,
                                                const String& preferredName,
                                                uint32_t scanSeconds) {
    if (_client == nullptr) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::InvalidState), "scanCamera");
        return RicohBleDeviceInfo{};
    }

    publish(AppEventType::BleScanStarted, 0, "scanCamera");
    RicohBleDeviceInfo info = _client->scanForCamera(preferredAddress, preferredName, scanSeconds);
    if (info.found) {
        LOGI("BLE", "camera candidate found addr=%s rssi=%d connectable=%d",
             info.address.c_str(),
             info.rssi,
             info.connectable ? 1 : 0);
        publish(AppEventType::BleDeviceFound, info.rssi, "scanCamera");
    }
    return info;
}

bool BleCameraService::isBonded(const RicohBleDeviceInfo& info) {
    return _client != nullptr && _client->isBonded(info);
}

Result BleCameraService::connect() {
    Result ready = requireClient("connect");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "connect");
    }
    return ready;
}

Result BleCameraService::connectCamera(const RicohBleDeviceInfo& info, uint32_t timeoutMs) {
    Result ready = requireClient("connectCamera");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "connectCamera");
        return ready;
    }
    if (!_client->connect(info, timeoutMs)) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::BleConnectFailed), "connectCamera");
        return Result::failure(ErrorCode::BleConnectFailed, _client->lastError());
    }

    LOGI("BLE", "connected to camera addr=%s", info.address.c_str());
    publish(AppEventType::BleConnected, 0, "connectCamera");
    return Result::success();
}

Result BleCameraService::connectCamera(const RicohBleDeviceInfo& info, const RicohBleConnectOptions& options) {
    Result ready = requireClient("connectCamera");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "connectCamera");
        return ready;
    }
    if (!_client->connect(info, options)) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::BleConnectFailed), "connectCamera");
        return Result::failure(ErrorCode::BleConnectFailed, _client->lastError());
    }

    LOGI("BLE", "connected to camera addr=%s", info.address.c_str());
    publish(AppEventType::BleConnected, 0, "connectCamera");
    return Result::success();
}

void BleCameraService::disconnect() {
    if (_client != nullptr) {
        const bool wasConnected = _client->isConnected();
        _client->disconnect();
        if (wasConnected) {
            publish(AppEventType::BleDisconnected, 0, "disconnect");
        }
    }
}

bool BleCameraService::isConnected() const {
    return _client != nullptr && _client->isConnected();
}

bool BleCameraService::shutterReady() const {
    return _client != nullptr && _client->shutterReady();
}

Result BleCameraService::shoot(bool autofocus) {
    Result ready = requireClient("shoot");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "shoot");
        return ready;
    }
    if (!_client->shoot(autofocus)) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::ShutterFailed), "shoot");
        return Result::failure(ErrorCode::ShutterFailed, _client->lastError());
    }

    publish(AppEventType::ShutterPressed, autofocus ? 1 : 0, "shoot");
    return Result::success();
}

Result BleCameraService::openWifi() {
    Result ready = requireClient("openWifi");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "openWifi");
        return ready;
    }
    if (!_client->openWifi()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::BleConnectFailed), "openWifi");
        return Result::failure(ErrorCode::BleConnectFailed, _client->lastError());
    }

    publish(AppEventType::WifiActivationRequested, 0, "openWifi");
    return Result::success();
}

Result BleCameraService::readPowerState(RicohCameraPowerState& state) {
    Result ready = requireClient("readPowerState");
    if (ready.failed()) {
        state = RicohCameraPowerState::Unknown;
        publishPowerState(state);
        return ready;
    }
    if (!_client->readPowerState(state)) {
        state = RicohCameraPowerState::Unknown;
        publishPowerState(state);
        return Result::failure(ErrorCode::Unknown, _client->lastError());
    }

    publishPowerState(state);
    return Result::success();
}

Result BleCameraService::readOperationMode(RicohCameraOperationMode& mode) {
    Result ready = requireClient("readOperationMode");
    if (ready.failed()) {
        mode = RicohCameraOperationMode::Unknown;
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "readOperationMode");
        return ready;
    }
    if (!_client->readOperationMode(mode)) {
        mode = RicohCameraOperationMode::Unknown;
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::Unknown), "readOperationMode");
        return Result::failure(ErrorCode::Unknown, _client->lastError());
    }
    return Result::success();
}

Result BleCameraService::enablePowerStateNotify() {
    Result ready = requireClient("enablePowerStateNotify");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "enablePowerStateNotify");
        return ready;
    }
    if (!_client->enablePowerStateNotify()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::Unknown), "enablePowerStateNotify");
        return Result::failure(ErrorCode::Unknown, _client->lastError());
    }
    return Result::success();
}

bool BleCameraService::consumePowerOffNotification() {
    const bool notified = _client != nullptr && _client->consumePowerOffNotification();
    if (notified) {
        publish(AppEventType::CameraPowerOff, 0, "power notify");
    }
    return notified;
}

Result BleCameraService::waitForWifiCredentials(RicohBleWifiCredentials& credentials, uint32_t timeoutMs) {
    Result ready = requireClient("waitForWifiCredentials");
    if (ready.failed()) {
        credentials = RicohBleWifiCredentials{};
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "waitForWifiCredentials");
        return ready;
    }
    if (!_client->waitForWifiCredentials(credentials, timeoutMs)) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::Timeout), "waitForWifiCredentials");
        return Result::failure(ErrorCode::Timeout, _client->lastError());
    }
    return Result::success();
}

int BleCameraService::consumeDisconnectReason() {
    const int reason = _client != nullptr ? _client->consumeDisconnectReason() : 0;
    if (reason != 0) {
        publish(AppEventType::BleDisconnected, reason, "disconnect reason");
    }
    return reason;
}

void BleCameraService::clearDisconnectReason() {
    if (_client != nullptr) {
        _client->clearDisconnectReason();
    }
}

Result BleCameraService::deleteAllBonds() {
    Result ready = requireClient("deleteAllBonds");
    if (ready.failed()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ready.code), "deleteAllBonds");
        return ready;
    }
    if (!_client->deleteAllBonds()) {
        publish(AppEventType::ErrorRaised, static_cast<int>(ErrorCode::Unknown), "deleteAllBonds");
        return Result::failure(ErrorCode::Unknown, _client->lastError());
    }
    return Result::success();
}

void BleCameraService::resetStack(bool clearObjects) {
    if (_client != nullptr) {
        _client->resetStack(clearObjects);
    }
}

bool BleCameraService::lastFailureWasResourceExhausted() const {
    return _client != nullptr && _client->lastFailureWasResourceExhausted();
}

String BleCameraService::statusText() const {
    return _client != nullptr ? _client->statusText() : String("BLE service not attached");
}

String BleCameraService::lastError() const {
    return _client != nullptr ? _client->lastError() : String("BLE service not attached");
}

Result BleCameraService::requireClient(const char* operation) const {
    if (_client != nullptr) {
        return Result::success();
    }
    String message("BLE service not attached");
    if (operation != nullptr && operation[0] != '\0') {
        message += ": ";
        message += operation;
    }
    return Result::failure(ErrorCode::InvalidState, message);
}

void BleCameraService::publish(AppEventType type, int code, const char* detail) {
    _pendingEvent.type = type;
    _pendingEvent.timestampMs = millis();
    _pendingEvent.code = code;
    _pendingEvent.detail = detail;
    _hasPendingEvent = true;
}

void BleCameraService::publishPowerState(RicohCameraPowerState state) {
    switch (state) {
        case RicohCameraPowerState::On:
            publish(AppEventType::CameraPowerOn, static_cast<int>(state), "readPowerState");
            return;
        case RicohCameraPowerState::OffOrShuttingDown:
            publish(AppEventType::CameraPowerOff, static_cast<int>(state), "readPowerState");
            return;
        case RicohCameraPowerState::Unknown:
            publish(AppEventType::CameraPowerUnknown, static_cast<int>(state), "readPowerState");
            return;
    }
    publish(AppEventType::CameraPowerUnknown, static_cast<int>(RicohCameraPowerState::Unknown), "readPowerState");
}

}  // namespace rvf
