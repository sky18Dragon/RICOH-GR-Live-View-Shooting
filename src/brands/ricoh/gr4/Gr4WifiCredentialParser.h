#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../common/RicohWifiCredentials.h"

// Merges one GR IV WLAN parameter read into `out`. `label` names the handle
// it came from: "ssid", "passphrase", "security", "frequency" or "bssid".
// Values may be plain text, key/value text or raw bytes; `out.valid` is
// recomputed after every merge.
void mergeGr4CredentialValue(RicohWifiCredentials& out, const char* label, const std::vector<uint8_t>& data);

// Exposed for tests.
std::string gr4PrintableText(const std::vector<uint8_t>& data);
std::string gr4MaybeQuotedValue(const std::string& text, const char* key);
std::string gr4MacFromText(const std::string& text);
uint16_t gr4FrequencyFromValue(const std::string& text, const std::vector<uint8_t>& data);
