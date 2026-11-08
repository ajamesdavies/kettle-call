#pragma once

#include <Arduino.h>

// =====================
// Project identity
// =====================

constexpr char ProjectName[] = "KettleCall";


constexpr uint8_t VersionMajor = 0;
constexpr uint8_t VersionMinor = 5;
constexpr uint8_t VersionPatch = 2;

// =====================
// WiFi
// =====================

#include "LocalConfig.h"

// =====================
// Network
// =====================

constexpr uint16_t KettleCallPort = 5005;
constexpr char MdnsServiceName[] = "kettlecall";
constexpr char MdnsProtocol[] = "udp";

// =====================
// Hardware
// =====================

constexpr uint8_t LED_PIN = 48;
constexpr uint8_t LED_COUNT = 1;

constexpr uint8_t PTT_PIN = 10;

// =====================
// I2S Microphone - INMP441
// =====================

constexpr int I2S_MIC_SCK = 5;
constexpr int I2S_MIC_WS  = 6;
constexpr int I2S_MIC_SD  = 4;

// =====================
// I2S Speaker - MAX98357A
// =====================

constexpr int I2S_SPK_BCLK = 17;
constexpr int I2S_SPK_LRC  = 18;
constexpr int I2S_SPK_DIN  = 7;
constexpr int I2S_AMP_SD = 11;

// =====================
// Audio
// =====================

constexpr uint32_t AudioSampleRate = 16000;
constexpr size_t AudioFrameSamples = 320; // 20 ms at 16 kHz