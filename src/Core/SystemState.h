#pragma once

#include <Arduino.h>
#include <IPAddress.h>


enum class LedState
{
    Boot,
    Searching,
    Ready,
    TX,
    RX,
    Error
};

struct SystemState
{
    bool wifiConnected = false;
    bool peerConnected = false;

    bool transmitting = false;
    bool receiving = false;

    IPAddress peerIP = IPAddress(0, 0, 0, 0);
    char peerName[32] = {0};

    int rssi = 0;

    uint32_t txPackets = 0;
    uint32_t rxPackets = 0;
    uint32_t badPackets = 0;

    uint32_t keepAliveSent = 0;
    uint32_t keepAliveReceived = 0;

    uint32_t lastPeerSeenMs = 0;

    uint32_t lostPackets = 0;

    uint32_t uptime = 0;
    uint32_t freeHeap = 0;

    uint32_t micFrames = 0;
    uint32_t micReadErrors = 0;
    int32_t micPeak = 0;

    uint32_t audioPacketsSent = 0;
    uint32_t audioPacketsReceived = 0;
    uint32_t audioTxDropped = 0;
    uint32_t audioRxBad = 0;
    uint32_t audioRxQueued = 0;

    uint32_t audioRxDequeued = 0;
    uint32_t audioRxQueueDropped = 0;
    uint32_t audioRxQueueDepth = 0;

    int32_t txAudioPeak = 0;
    int32_t rxAudioPeak = 0;
    int32_t speakerAudioPeak = 0;

    uint32_t speakerFramesWritten = 0;
    uint32_t speakerSilenceFrames = 0;
    uint32_t speakerWriteErrors = 0;

    LedState ledState = LedState::Boot;
};

extern SystemState System;