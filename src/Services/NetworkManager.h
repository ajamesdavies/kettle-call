#pragma once

#include <Arduino.h>
#include <IPAddress.h>

namespace NetworkManager
{
    void begin();

    bool wifiConnected();

    bool peerConnected();

    IPAddress localIP();

    IPAddress peerIP();

    bool queueAudioFrame(const int16_t* samples, uint16_t sampleCount);
}