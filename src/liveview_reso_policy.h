#pragma once

#include <cstddef>
#include <cstdint>

struct LiveViewResolution {
  uint16_t width = 0;
  uint16_t height = 0;

  bool valid() const { return width > 0 && height > 0; }
};

constexpr size_t LIVEVIEW_RESO_LIST_CAPACITY = 8;

// Collects every "<width>x<height>" token inside the /v1/props "resoList"
// array, in the order the camera listed them. Returns the number stored.
size_t parseLiveViewResoList(const char* json, LiveViewResolution* out, size_t capacity);

// Picks the smallest listed resolution that still covers minWidth x minHeight
// at that aspect ratio (within 1%, since the decoder scales to a fixed size)
// and is strictly smaller than the camera's default stream. Returns an invalid
// resolution otherwise, so the default is never requested by name.
LiveViewResolution chooseLiveViewResolution(const LiveViewResolution* list,
                                            size_t count,
                                            uint16_t minWidth,
                                            uint16_t minHeight,
                                            uint16_t defaultWidth,
                                            uint16_t defaultHeight);
