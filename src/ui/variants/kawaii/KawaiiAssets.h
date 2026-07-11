#pragma once

#include <cstddef>
#include <cstdint>

namespace rvf::ui {

struct KawaiiPatternBlob {
    int16_t x;
    int16_t y;
    int16_t radius;
};

// Code-drawn assets keep the theme lightweight and avoid runtime bitmap
// decoding or a large full-screen image in Flash.
struct KawaiiAssets {
    inline static constexpr KawaiiPatternBlob kPatternBlobs[] = {
        {14, 12, 7}, {45, 5, 9},   {78, 18, 6},  {111, 6, 8},
        {147, 17, 9}, {181, 7, 6}, {218, 16, 9}, {27, 48, 8},
        {67, 39, 7},  {201, 45, 8}, {232, 62, 6}, {11, 83, 9},
        {53, 96, 6},  {187, 92, 9}, {222, 111, 7}, {89, 125, 8},
        {153, 121, 6}, {18, 128, 5},
    };

    static constexpr size_t kPatternBlobCount =
        sizeof(kPatternBlobs) / sizeof(kPatternBlobs[0]);
    static constexpr const char* kFirmwareLabel = "FW 1.0.1";
    static constexpr const char* kThemeLabel = "Kawaii Theme";
};

}  // namespace rvf::ui
