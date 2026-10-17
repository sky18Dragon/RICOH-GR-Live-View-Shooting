#include "liveview_reso_policy.h"

#include <cstring>

namespace {

bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

const char* readNumber(const char* cursor, const char* end, uint32_t& value) {
  value = 0;
  const char* start = cursor;
  while (cursor < end && isDigit(*cursor)) {
    value = value * 10 + static_cast<uint32_t>(*cursor - '0');
    if (value > 0xFFFF) {
      return nullptr;
    }
    ++cursor;
  }
  return cursor == start ? nullptr : cursor;
}

}  // namespace

size_t parseLiveViewResoList(const char* json, LiveViewResolution* out, size_t capacity) {
  if (json == nullptr || out == nullptr || capacity == 0) {
    return 0;
  }
  const char* key = std::strstr(json, "\"resoList\"");
  if (key == nullptr) {
    return 0;
  }
  const char* open = std::strchr(key, '[');
  if (open == nullptr) {
    return 0;
  }
  const char* end = std::strchr(open, ']');
  if (end == nullptr) {
    return 0;
  }

  size_t count = 0;
  const char* cursor = open + 1;
  while (cursor < end && count < capacity) {
    if (!isDigit(*cursor)) {
      ++cursor;
      continue;
    }
    uint32_t width = 0;
    uint32_t height = 0;
    const char* afterWidth = readNumber(cursor, end, width);
    if (afterWidth == nullptr) {
      while (cursor < end && isDigit(*cursor)) {
        ++cursor;
      }
      continue;
    }
    if (afterWidth < end && (*afterWidth == 'x' || *afterWidth == 'X')) {
      const char* afterHeight = readNumber(afterWidth + 1, end, height);
      if (afterHeight != nullptr && width > 0 && height > 0) {
        out[count].width = static_cast<uint16_t>(width);
        out[count].height = static_cast<uint16_t>(height);
        ++count;
        cursor = afterHeight;
        continue;
      }
    }
    cursor = afterWidth;
  }
  return count;
}

LiveViewResolution chooseLiveViewResolution(const LiveViewResolution* list,
                                            size_t count,
                                            uint16_t minWidth,
                                            uint16_t minHeight,
                                            uint16_t defaultWidth,
                                            uint16_t defaultHeight) {
  LiveViewResolution smallest;
  if (list == nullptr || minWidth == 0 || minHeight == 0) {
    return smallest;
  }
  const uint32_t defaultArea = static_cast<uint32_t>(defaultWidth) * defaultHeight;
  for (size_t i = 0; i < count; ++i) {
    const LiveViewResolution& candidate = list[i];
    if (!candidate.valid() || candidate.width < minWidth || candidate.height < minHeight) {
      continue;
    }
    if (static_cast<uint32_t>(candidate.width) * candidate.height >= defaultArea) {
      continue;
    }
    const uint32_t scaledWidth = static_cast<uint32_t>(candidate.width) * minHeight;
    const uint32_t scaledHeight = static_cast<uint32_t>(candidate.height) * minWidth;
    const uint32_t aspectError = scaledWidth > scaledHeight ? scaledWidth - scaledHeight : scaledHeight - scaledWidth;
    if (aspectError * 100 > scaledWidth) {
      continue;
    }
    const uint32_t area = static_cast<uint32_t>(candidate.width) * candidate.height;
    if (!smallest.valid() || area < static_cast<uint32_t>(smallest.width) * smallest.height) {
      smallest = candidate;
    }
  }
  return smallest;
}
