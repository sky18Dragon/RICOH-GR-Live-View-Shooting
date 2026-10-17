#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct Gr2ProvisioningForm {
  Gr2ProvisioningForm(const std::string& selected,
                      const std::string& manual,
                      const std::string& password,
                      const std::string& setupSsid)
      : selectedSsid(selected),
        manualSsid(manual),
        passphrase(password),
        setupAccessPointSsid(setupSsid) {}

  std::string selectedSsid;
  std::string manualSsid;
  std::string passphrase;
  std::string setupAccessPointSsid;
};

struct Gr2ProvisioningValidation {
  Gr2ProvisioningValidation() : valid(false) {}
  Gr2ProvisioningValidation(bool isValid,
                            const std::string& selectedSsid,
                            const std::string& selectedPassphrase)
      : valid(isValid), ssid(selectedSsid), passphrase(selectedPassphrase) {}

  bool valid;
  std::string ssid;
  std::string passphrase;
};

struct Gr2ScannedNetwork {
  Gr2ScannedNetwork(const std::string& networkSsid, int signalStrength)
      : ssid(networkSsid), rssi(signalStrength) {}

  std::string ssid;
  int rssi;
};

struct Gr2NetworkOptions {
  Gr2NetworkOptions() : ricohFound(false), optionCount(0) {}

  std::string html;
  bool ricohFound;
  size_t optionCount;
};

Gr2ProvisioningValidation validateGr2ProvisioningForm(const Gr2ProvisioningForm& form);
Gr2NetworkOptions renderGr2NetworkOptions(const std::vector<Gr2ScannedNetwork>& networks,
                                          const std::string& setupAccessPointSsid,
                                          bool scanHadResults);
bool gr2CredentialsReadyForHandoff(bool portalActive,
                                   bool submitted,
                                   uint32_t submittedAtMs,
                                   uint32_t nowMs,
                                   uint32_t responseFlushMs);
