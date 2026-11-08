#pragma once

#include <stdint.h>

namespace AudioManager
{
    void begin();
    bool ready();

    bool queueReceivedFrame(const int16_t* samples, uint16_t sampleCount);
}