#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

struct Gr2ProvisionedWifi {
  String ssid;
  String passphrase;
};

class Gr2ProvisioningPortal {
public:
  bool begin();
  void loop();
  void stop();

  bool active() const { return _active; }
  bool takeCredentials(Gr2ProvisionedWifi& credentials);
  const String& accessPointSsid() const { return _accessPointSsid; }
  const String& accessPointPassword() const { return _accessPointPassword; }
  String accessPointIp() const;
  const String& lastError() const { return _lastError; }

private:
  void configureRoutes();
  void handleRoot();
  void handleSave();
  void redirectToRoot();
  String scanNetworkOptions();
  String pageShell(const String& content) const;

  DNSServer _dns;
  WebServer _server{80};
  String _accessPointSsid;
  String _accessPointPassword;
  String _networkOptions;
  String _lastError;
  Gr2ProvisionedWifi _pending;
  uint32_t _submittedAtMs = 0;
  bool _routesConfigured = false;
  bool _active = false;
  bool _submitted = false;
  uint8_t _lastStationCount = 0;
};
