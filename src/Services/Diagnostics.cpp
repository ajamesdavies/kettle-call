#include "Services/Diagnostics.h"

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/SystemState.h"
#include "Core/TaskManager.h"

extern SystemState System;

namespace
{
    constexpr uint32_t DiagnosticsIntervalMs = 5000;
    constexpr uint32_t StartupDelayMs = 3000;

    void diagnosticsTask(void*)
    {
        // Let the boot banner and network startup logs finish first.
        vTaskDelay(pdMS_TO_TICKS(StartupDelayMs));

        while (true)
        {
            System.uptime = millis() / 1000;
            System.freeHeap = ESP.getFreeHeap();

            const char* peer =
                System.peerConnected && System.peerName[0] != '\0'
                    ? System.peerName
                    : "Searching";

Logger::info(
    "DIAG",
    "Device=%s Heap=%u Uptime=%lus WiFi=%s RSSI=%d Peer=%s PTT=%s Mic=%ld Frames=%u Err=%u Aud=%u/%u TxDrop=%u RxQ=%u/%u QD=%u RxDrop=%u Spk=%u Sil=%u SpkErr=%u TxPk=%ld RxPk=%ld SpkPk=%ld TX=%u RX=%u KA=%u/%u Bad=%u",
    DEVICE_NAME,
    System.freeHeap,
    System.uptime,
    System.wifiConnected ? "OK" : "DOWN",
    System.rssi,
    peer,
    System.transmitting ? "DOWN" : "UP",
    System.micPeak,
    System.micFrames,
    System.micReadErrors,
    System.audioPacketsSent,
    System.audioPacketsReceived,
    System.audioTxDropped,
    System.audioRxQueued,
    System.audioRxDequeued,
    System.audioRxQueueDepth,
    System.audioRxQueueDropped,
    System.speakerFramesWritten,
    System.speakerSilenceFrames,
    System.speakerWriteErrors,
    System.txAudioPeak,
    System.rxAudioPeak,
    System.speakerAudioPeak,
    System.txPackets,
    System.rxPackets,
    System.keepAliveSent,
    System.keepAliveReceived,
    System.badPackets);

            vTaskDelay(pdMS_TO_TICKS(DiagnosticsIntervalMs));
        }
    }
}

void Diagnostics::begin()
{
    bool created = TaskManager::create(
        "Diagnostics",
        diagnosticsTask,
        nullptr,
        4096,
        1,
        0);

    if (!created)
    {
        Logger::error("DIAG", "Failed to create diagnostics task");
    }
}