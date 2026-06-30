#pragma once

#include <cstddef>
#include <cstdint>

class MjpegStream {
public:
  using FrameCallback = void (*)(const uint8_t* data, size_t len, void* user);

  bool begin(uint8_t* frameBuf, size_t cap, FrameCallback cb, void* user);
  void reset();
  size_t process(const uint8_t* data, size_t len);

  uint32_t frames() const;
  uint32_t droppedFrames() const;
  size_t currentLength() const;

private:
  enum class State : uint8_t {
    SeekingSoi,
    InFrame,
  };

  void startFrame();
  void appendByte(uint8_t value);
  void finishFrame();
  void dropFrame();

  uint8_t* _frameBuf = nullptr;
  size_t _cap = 0;
  size_t _len = 0;
  FrameCallback _callback = nullptr;
  void* _user = nullptr;
  State _state = State::SeekingSoi;
  uint8_t _prev = 0;
  bool _havePrev = false;
  bool _overflow = false;
  uint32_t _frames = 0;
  uint32_t _droppedFrames = 0;
};
