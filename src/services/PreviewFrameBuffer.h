#pragma once

#include <Arduino.h>

#include "PreviewFrameBufferStorage.h"

namespace rvf {

struct PreviewFrameStats {
    bool attached = false;
    PreviewFrameBufferStorage storage = PreviewFrameBufferStorage::Unknown;
    size_t capacity = 0;
    uint32_t streamFrames = 0;
    uint32_t renderedFrames = 0;
    uint32_t droppedFrames = 0;
    size_t currentFrameLength = 0;
    size_t lastFrameSize = 0;
    size_t maxFrameSize = 0;
    uint32_t totalFrameBytes = 0;
    uint32_t freeInternalBytes = 0;
    uint32_t largestInternalBlockBytes = 0;
    uint32_t freePsramBytes = 0;
};

class PreviewFrameBuffer {
public:
    bool attach(uint8_t* buffer, size_t capacity, PreviewFrameBufferStorage storage);
    void reset();
    void resetRuntimeStats();

    uint8_t* data() const { return _buffer; }
    size_t capacity() const { return _stats.capacity; }
    bool attached() const { return _buffer != nullptr && _stats.capacity > 0; }

    void recordFrame(size_t size);
    void syncStreamStats(uint32_t streamFrames, uint32_t droppedFrames, size_t currentLength);
    void captureMemoryStats();
    void logStatsIfDue(uint32_t nowMs, uint32_t intervalMs = 10000);

    PreviewFrameStats stats() const { return _stats; }
    const char* storageName() const;

private:
    uint8_t* _buffer = nullptr;
    PreviewFrameStats _stats;
    uint32_t _lastStatsLogMs = 0;
};

}  // namespace rvf
