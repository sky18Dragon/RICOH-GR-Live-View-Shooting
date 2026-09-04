#include "gr2_provisioning_logic.h"

#include <cctype>

namespace {

std::string trimCopy(const std::string& value) {
  size_t begin = 0;
  while (begin < value.length() &&
         std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  size_t end = value.length();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(begin, end - begin);
}

bool validPassphraseLength(size_t length) {
  return length == 0 || (length >= 8 && length <= 63);
}

bool startsWithRicoh(const std::string& ssid) {
  return ssid.rfind("RICOH_", 0) == 0;
}

std::string escapeHtml(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.length() + 8);
  for (const char character : value) {
    switch (character) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += character; break;
    }
  }
  return escaped;
}

}  // namespace

Gr2ProvisioningValidation validateGr2ProvisioningForm(const Gr2ProvisioningForm& form) {
  const std::string manualSsid = trimCopy(form.manualSsid);
  const std::string ssid = manualSsid.empty() ? form.selectedSsid : manualSsid;
  if (ssid.empty() || ssid.length() > 32 || ssid == form.setupAccessPointSsid ||
      !validPassphraseLength(form.passphrase.length())) {
    return {};
  }
  return {true, ssid, form.passphrase};
}

Gr2NetworkOptions renderGr2NetworkOptions(const std::vector<Gr2ScannedNetwork>& networks,
                                          const std::string& setupAccessPointSsid,
                                          bool scanHadResults) {
  Gr2NetworkOptions result;
  if (!scanHadResults) {
    result.html = "<option value=''>未找到网络，请确认相机 Wi-Fi 已开启</option>";
    return result;
  }

  for (int pass = 0; pass < 2; ++pass) {
    for (const Gr2ScannedNetwork& network : networks) {
      if (network.ssid.empty() || network.ssid == setupAccessPointSsid) {
        continue;
      }
      const bool ricoh = startsWithRicoh(network.ssid);
      if ((pass == 0) != ricoh) {
        continue;
      }
      result.ricohFound = result.ricohFound || ricoh;
      ++result.optionCount;
      const std::string escapedSsid = escapeHtml(network.ssid);
      result.html += "<option value='" + escapedSsid + "'>";
      if (ricoh) {
        result.html += "★ ";
      }
      result.html += escapedSsid + " (" + std::to_string(network.rssi) + " dBm)</option>";
    }
  }

  if (result.optionCount == 0) {
    result.html = "<option value=''>未找到可用网络</option>";
  } else if (!result.ricohFound) {
    result.html = "<option value=''>未识别到 RICOH_ 热点，请确认后再选择</option>" + result.html;
  }
  return result;
}

bool gr2CredentialsReadyForHandoff(bool portalActive,
                                   bool submitted,
                                   uint32_t submittedAtMs,
                                   uint32_t nowMs,
                                   uint32_t responseFlushMs) {
  return portalActive && submitted && (nowMs - submittedAtMs) >= responseFlushMs;
}
