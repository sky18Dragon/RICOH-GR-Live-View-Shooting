# Preview Lag Investigation

## 2026-08-01 GR IIIx HDF hardware results

Test hardware:

- M5Stack StickS3, ESP32-S3-PICO-1-N8R8 at 240 MHz.
- RICOH GR IIIx HDF connected over the camera's 2.4 GHz AP (channel 6).
- Camera LiveView JPEGs are 720×480 and approximately 30–35 KiB each.
- The ESP32-S3 has no hardware JPEG decoder. The final build uses Espressif's
  SIMD-optimized `esp_new_jpeg` 1.0.2 software decoder.

All measurements below are five-second serial-stat windows after warm-up. The
quarter-scale stability run reached 1,541 frames, and the final direct-scale
PSRAM-output run reached 850 frames; neither had an MJPEG overflow/drop, stall,
or memory decline.

| Variant | Preview fps | JPEG decode | Decode + overlay + LCD | Stream rate |
| --- | ---: | ---: | ---: | ---: |
| Original: half scale, 2 KiB reads, PSRAM canvas | 3.5–4.7 | 109–112 ms | 123–127 ms | 122–143 KiB/s |
| Quarter scale, 2 KiB reads, PSRAM canvas | 7.0–7.2 | 39–40 ms | 54–55 ms | 216–218 KiB/s |
| Quarter scale, 2 KiB reads, internal canvas | 7.4–7.6 | 34–35 ms | 47–48 ms | 226–230 KiB/s |
| Quarter scale, 8 KiB reads, internal canvas | 10.2–10.5 | 34–35 ms | 47–48 ms | 311–319 KiB/s |
| `esp_new_jpeg` 216×144 crop, internal output | 9.7–9.8 | 35.5–36.2 ms | 48.7–49.3 ms | 295–300 KiB/s |
| Final: `esp_new_jpeg` 216×144 crop, PSRAM output | 9.4–9.6 | 37.9–38.2 ms | 51.1–51.4 ms | 288–294 KiB/s |

The 216×144 image is center-cropped to 216×135 (four rows from the top and five
from the bottom), centered with 12-pixel side bars, and copied without software
resampling. The internal-output experiment reached 1,356 frames with no drops,
but left only about 16 KiB internal RAM free. The final PSRAM-output run reached
850 frames with no drops and retained about 78 KiB free internal RAM with a
49 KiB largest block.

## Conclusions

1. JPEGDEC's `JPEG_SCALE_HALF` decodes 360×240 pixels from the 720×480 source before fitting to a 203×135 display rectangle. `JPEG_SCALE_QUARTER` decodes 180×120 and only needs a small upscale, cutting decode time by about 64%.
2. A PSRAM-backed M5GFX sprite disables its DMA path. A landscape-only internal-RAM canvas reduced decode/render latency by another 6–7 ms while retaining a safe measured heap margin.
3. The 2 KiB receive buffer forced roughly 15 read/process/tick cycles per 31 KiB JPEG. Because the main tick ends in `delay(1)`, this throttled socket draining. An 8 KiB buffer reduced those gaps and exposed the camera's practical ~10 fps stream rate.
4. The existing BLE parking remains valuable. Modem sleep, however, cannot be disabled merely after disconnecting the BLE link: ESP-IDF aborts while the Bluetooth controller remains enabled. The attempted setting was reverted.
5. The LCD is already driven at 40 MHz. A 240×135 RGB565 transfer has a theoretical 13 ms wire time, matching the measured 13–14 ms beyond decode, so raising display throughput is not a promising safe lever.
6. `esp_new_jpeg` can scale directly to dimensions divisible by eight. A 216×144
   decode plus center crop removes the 180×120 nearest-neighbor upscale while
   retaining approximately 95% of the quarter-scale path's frame rate. Keeping
   its 60.75 KiB output buffer in PSRAM costs about 2.5 ms per frame but restores
   62 KiB of internal heap headroom.

## Protocol findings

- `GET /v1/liveview` provides a continuous sequence of JPEG images rather than a documented configurable stream profile.
- The tested GR IIIx HDF emits 720×480 JPEGs at approximately 10.4 fps once the client drains TCP promptly.
- No supported resolution or frame-rate negotiation is known for this undocumented endpoint. Query-parameter guessing was not shipped because it would be unverified camera-specific behavior.

## Final implementation

- `src/config.h`: `JPEG_DECODE_WIDTH=216`, `JPEG_DECODE_HEIGHT=144`, and `STREAM_READ_BUFFER_SIZE=8192`.
- `src/jpeg_decoder.*`: `esp_new_jpeg` RGB565-BE decode to a persistent,
  16-byte-aligned PSRAM buffer, then a centered 216×135 crop with no resampling.
- `platformio.ini` / `scripts/link_esp_new_jpeg.py`: fetch and link the pinned
  Espressif Component Registry binary for ESP32-S3.
- `src/display.cpp`: internal-RAM/DMA landscape canvas with PSRAM fallback; portrait remains in PSRAM.
- `src/services/WifiPreviewService.*`: five-second aggregate and maximum decode/render telemetry, dimensions, and KiB/s.
- Camera power protection, BLE/Wi-Fi guards, and the LiveView stall watchdog remain unchanged.

## Follow-up hardware checks

- Run a 10/30/60 minute soak when convenient.
- Visually confirm the slight vertical crop is preferable to the old quarter-scale upscale.
- Repeat on GR III and GR IV because their LiveView resolution/rate may differ.
