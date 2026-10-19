#include "Gr4WifiCredentialParser.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Port of the Arduino String parser that used to live in ricoh_ble_client.cpp.
// trim / case conversion / indexOf / substring keep String's semantics.

namespace {

void trim(std::string& text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  text = text.substr(start, end - start);
}

std::string lowerCopy(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

std::string upperCopy(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return text;
}

bool startsWith(const std::string& text, const char* prefix) {
  return text.compare(0, std::strlen(prefix), prefix) == 0;
}

bool looksLikeSsid(const std::string& text) {
  if (text.empty() || text.size() > 32) {
    return false;
  }
  const std::string upper = upperCopy(text);
  return startsWith(upper, "GR_") || startsWith(upper, "RICOH_GR") ||
         startsWith(upper, "GR4_") || startsWith(upper, "GR_4_");
}

bool isHexPair(char a, char b) {
  return std::isxdigit(static_cast<unsigned char>(a)) && std::isxdigit(static_cast<unsigned char>(b));
}

std::string macFromRaw6(const std::vector<uint8_t>& data) {
  if (data.size() != 6) {
    return std::string();
  }
  char mac[18];
  std::snprintf(mac,
                sizeof(mac),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                data[0],
                data[1],
                data[2],
                data[3],
                data[4],
                data[5]);
  return std::string(mac);
}

uint16_t frequencyMhzForChannel(uint8_t channel) {
  if (channel >= 1 && channel <= 13) {
    return static_cast<uint16_t>(2407 + channel * 5);
  }
  if (channel == 14) {
    return 2484;
  }
  return 0;
}

uint8_t channelFromFrequencyMhz(uint16_t frequencyMhz) {
  if (frequencyMhz == 2484) {
    return 14;
  }
  if (frequencyMhz >= 2412 && frequencyMhz <= 2472 && ((frequencyMhz - 2407) % 5) == 0) {
    return static_cast<uint8_t>((frequencyMhz - 2407) / 5);
  }
  return 0;
}

uint16_t normalizeFrequencyOrChannel(uint32_t value) {
  if (value >= 2412 && value <= 2484) {
    return static_cast<uint16_t>(value);
  }
  if (value >= 1 && value <= 14) {
    return frequencyMhzForChannel(static_cast<uint8_t>(value));
  }
  return 0;
}

uint16_t frequencyFromText(const std::string& text) {
  const char* cursor = text.c_str();
  while (*cursor != '\0') {
    while (*cursor != '\0' && !std::isdigit(static_cast<unsigned char>(*cursor))) {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    char* end = nullptr;
    const unsigned long value = std::strtoul(cursor, &end, 10);
    const uint16_t normalized = normalizeFrequencyOrChannel(value);
    if (normalized != 0) {
      return normalized;
    }
    cursor = end != nullptr && end != cursor ? end : cursor + 1;
  }
  return 0;
}

uint32_t readUnsignedLe(const std::vector<uint8_t>& data, size_t width) {
  uint32_t value = 0;
  const size_t count = std::min(width, data.size());
  for (size_t i = 0; i < count; ++i) {
    value |= static_cast<uint32_t>(data[i]) << (8 * i);
  }
  return value;
}

uint32_t readUnsignedBe(const std::vector<uint8_t>& data, size_t width) {
  uint32_t value = 0;
  const size_t count = std::min(width, data.size());
  for (size_t i = 0; i < count; ++i) {
    value = (value << 8) | data[i];
  }
  return value;
}

uint16_t frequencyFromRaw(const std::vector<uint8_t>& data) {
  if (data.empty() || data.size() > 4) {
    return 0;
  }
  const uint16_t le = normalizeFrequencyOrChannel(readUnsignedLe(data, data.size()));
  if (le != 0) {
    return le;
  }
  return normalizeFrequencyOrChannel(readUnsignedBe(data, data.size()));
}

}  // namespace

std::string gr4PrintableText(const std::vector<uint8_t>& data) {
  std::string text;
  for (uint8_t b : data) {
    if (b == 0) {
      continue;
    }
    if (b == '\r' || b == '\n' || b == '\t') {
      text += ' ';
      continue;
    }
    if (b < 0x20 || b > 0x7E) {
      return std::string();
    }
    text += static_cast<char>(b);
  }
  trim(text);
  return text;
}

std::string gr4MaybeQuotedValue(const std::string& text, const char* key) {
  const std::string lowerText = lowerCopy(text);
  const std::string lowerKey = lowerCopy(key);

  const size_t keyPos = lowerText.find(lowerKey);
  if (keyPos == std::string::npos) {
    return std::string();
  }

  size_t sep = std::string::npos;
  for (size_t i = keyPos + lowerKey.size(); i < text.size(); ++i) {
    const char c = text[i];
    if (c == ':' || c == '=') {
      sep = i;
      break;
    }
    if (!(c == ' ' || c == '\t' || c == '"' || c == '\'')) {
      break;
    }
  }
  if (sep == std::string::npos) {
    return std::string();
  }

  size_t start = sep + 1;
  while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '"' || text[start] == '\'')) {
    ++start;
  }

  size_t end = start;
  while (end < text.size()) {
    const char c = text[end];
    if (c == '"' || c == '\'' || c == ',' || c == ';' || c == '}' || c == '\r' || c == '\n') {
      break;
    }
    ++end;
  }

  std::string value = text.substr(start, end - start);
  trim(value);
  return value;
}

std::string gr4MacFromText(const std::string& text) {
  for (size_t i = 0; i + 16 < text.size(); ++i) {
    if (isHexPair(text[i], text[i + 1]) &&
        (text[i + 2] == ':' || text[i + 2] == '-') &&
        isHexPair(text[i + 3], text[i + 4]) &&
        text[i + 5] == text[i + 2] &&
        isHexPair(text[i + 6], text[i + 7]) &&
        text[i + 8] == text[i + 2] &&
        isHexPair(text[i + 9], text[i + 10]) &&
        text[i + 11] == text[i + 2] &&
        isHexPair(text[i + 12], text[i + 13]) &&
        text[i + 14] == text[i + 2] &&
        isHexPair(text[i + 15], text[i + 16])) {
      std::string mac = text.substr(i, 17);
      std::replace(mac.begin(), mac.end(), '-', ':');
      return upperCopy(mac);
    }
  }
  return std::string();
}

uint16_t gr4FrequencyFromValue(const std::string& text, const std::vector<uint8_t>& data) {
  const uint16_t fromText = frequencyFromText(text);
  if (fromText != 0) {
    return fromText;
  }
  return frequencyFromRaw(data);
}

void mergeGr4CredentialValue(RicohWifiCredentials& out, const char* label, const std::vector<uint8_t>& data) {
  if (data.empty()) {
    return;
  }

  const std::string text = gr4PrintableText(data);
  if (!text.empty()) {
    std::string value = gr4MaybeQuotedValue(text, "ssid");
    if (looksLikeSsid(value)) {
      out.ssid = value;
    }

    value = gr4MaybeQuotedValue(text, "passphrase");
    if (value.empty()) {
      value = gr4MaybeQuotedValue(text, "password");
    }
    if (!value.empty()) {
      out.passphrase = value;
      out.encryptedPassphrase = false;
    }

    value = gr4MaybeQuotedValue(text, "bssid");
    if (value.empty()) {
      value = gr4MacFromText(text);
    }
    if (!value.empty()) {
      out.bssid = value;
    }
  }

  if (std::strcmp(label, "ssid") == 0 && out.ssid.empty() && looksLikeSsid(text)) {
    out.ssid = text;
  } else if (std::strcmp(label, "passphrase") == 0 && out.passphrase.empty()) {
    if (!text.empty()) {
      out.passphrase = text;
      out.encryptedPassphrase = false;
    } else {
      out.encryptedPassphrase = true;
    }
  } else if (std::strcmp(label, "bssid") == 0 && out.bssid.empty()) {
    std::string mac = !text.empty() ? gr4MacFromText(text) : std::string();
    if (mac.empty()) {
      mac = macFromRaw6(data);
    }
    if (!mac.empty()) {
      out.bssid = mac;
    }
  } else if (std::strcmp(label, "security") == 0 && data.size() == 1) {
    out.securityType = data[0];
  } else if (std::strcmp(label, "frequency") == 0) {
    const uint16_t frequencyMhz = gr4FrequencyFromValue(text, data);
    const uint8_t channel = channelFromFrequencyMhz(frequencyMhz);
    if (frequencyMhz != 0 && channel != 0) {
      out.frequencyMhz = frequencyMhz;
      out.channel = channel;
    }
  }

  out.valid = !out.ssid.empty() &&
              !out.encryptedPassphrase &&
              (!out.passphrase.empty() || out.securityType == 0);
}
