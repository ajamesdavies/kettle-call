#pragma once

#include <Arduino.h>

// "KCAL" = KettleCall
constexpr uint32_t KettleCallMagic = 0x4B43414C;

constexpr uint8_t KettleCallProtocolVersion = 1;

enum class PacketType : uint8_t
{
    Hello = 1,
    KeepAlive = 2,
    Audio = 3,
    Ack = 4,

    Diagnostics = 10,
    Config = 11,

    Unknown = 255
};

struct __attribute__((packed)) PacketHeader
{
    uint32_t magic;
    uint8_t protocolVersion;
    PacketType type;
    uint16_t payloadLength;
    uint32_t sequence;
    uint32_t timestampMs;
};

struct __attribute__((packed)) HelloPayload
{
    char deviceName[32];

    uint8_t versionMajor;
    uint8_t versionMinor;
    uint8_t versionPatch;

    int8_t rssi;
};

struct __attribute__((packed)) KeepAlivePayload
{
    char deviceName[32];

    uint32_t uptimeSeconds;
    int8_t rssi;
};

struct __attribute__((packed)) AckPayload
{
    uint32_t ackSequence;
};
constexpr uint16_t AudioPacketSamples = 320;

struct __attribute__((packed)) AudioPayload
{
    uint16_t sampleCount;
    uint16_t reserved;
    int16_t samples[AudioPacketSamples];
};