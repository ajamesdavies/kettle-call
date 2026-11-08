#include "Services/NetworkManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

#include <cctype>
#include <cstddef>
#include <cstring>

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/Packet.h"
#include "Core/SystemState.h"
#include "Core/TaskManager.h"

#include "Services/AudioManager.h"
#include "Drivers/LED.h"

extern SystemState System;

namespace
{
    WiFiUDP udp;

    char mdnsHostName[64] = {0};

    bool udpStarted = false;
    bool mdnsStarted = false;

    uint32_t lastPeerSearchMs = 0;
    uint32_t lastKeepAliveMs = 0;
    uint32_t txSequence = 0;

    constexpr uint32_t WifiRetryDelayMs = 250;
    constexpr uint32_t WifiLogIntervalMs = 2000;
    constexpr uint32_t PeerSearchIntervalMs = 5000;
    constexpr uint32_t KeepAliveIntervalMs = 1000;
    constexpr uint32_t PeerTimeoutMs = 8000;

    // Audio packets are much larger than keepalive packets.
    constexpr size_t MaxPacketSize = 768;

    constexpr uint8_t AudioTxQueueLength = 4;

    QueueHandle_t audioTxQueue = nullptr;

    uint8_t packetBuffer[MaxPacketSize];

    void setLedSearching()
    {
        System.ledState = LedState::Searching;
        LED::update();
    }

    void setLedReady()
    {
        System.ledState = LedState::Ready;
        LED::update();
    }

    void setLedError()
    {
        System.ledState = LedState::Error;
        LED::update();
    }

    void makeMdnsHostName()
    {
        const char prefix[] = "kettlecall-";

        memset(mdnsHostName, 0, sizeof(mdnsHostName));
        strncpy(mdnsHostName, prefix, sizeof(mdnsHostName) - 1);

        size_t pos = strlen(mdnsHostName);

        for (size_t i = 0; DEVICE_NAME[i] != '\0' && pos < sizeof(mdnsHostName) - 1; i++)
        {
            char c = DEVICE_NAME[i];

            if (isalnum(static_cast<unsigned char>(c)))
            {
                mdnsHostName[pos++] = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
            else if (c == ' ' || c == '-' || c == '_')
            {
                mdnsHostName[pos++] = '-';
            }
        }

        mdnsHostName[pos] = '\0';
    }

    int8_t currentRssi8()
    {
        int rssi = WiFi.RSSI();

        if (rssi < -128)
        {
            rssi = -128;
        }
        else if (rssi > 127)
        {
            rssi = 127;
        }

        return static_cast<int8_t>(rssi);
    }

    void copyDeviceName(char* destination, size_t destinationSize, const char* source)
    {
        if (destination == nullptr || destinationSize == 0)
        {
            return;
        }

        memset(destination, 0, destinationSize);

        if (source == nullptr)
        {
            strncpy(destination, "Unknown", destinationSize - 1);
            return;
        }

        strncpy(destination, source, destinationSize - 1);
    }

    void updatePeer(IPAddress ip, const char* name)
    {
        bool wasConnected = System.peerConnected;
        bool changedPeer = !wasConnected || System.peerIP != ip;

        System.peerIP = ip;
        copyDeviceName(System.peerName, sizeof(System.peerName), name);

        System.peerConnected = true;
        System.lastPeerSeenMs = millis();

        if (changedPeer)
        {
            Logger::info(
                "NETWORK",
                "Peer connected: %s at %s",
                System.peerName,
                System.peerIP.toString().c_str());
        }
    }

    bool connectWiFi()
    {
        Logger::info("NETWORK", "Device name: %s", DEVICE_NAME);
        Logger::info("NETWORK", "mDNS host: %s.local", mdnsHostName);
        Logger::info("NETWORK", "Connecting to WiFi SSID '%s'", WIFI_SSID);

        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.setHostname(mdnsHostName);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        uint32_t lastLogMs = 0;

        while (WiFi.status() != WL_CONNECTED)
        {
            uint32_t now = millis();

            if (now - lastLogMs >= WifiLogIntervalMs)
            {
                lastLogMs = now;
                Logger::info("NETWORK", "Still connecting to WiFi...");
            }

            vTaskDelay(pdMS_TO_TICKS(WifiRetryDelayMs));
        }

        System.wifiConnected = true;
        System.rssi = WiFi.RSSI();

        Logger::info(
            "NETWORK",
            "WiFi connected. IP=%s RSSI=%d dBm",
            WiFi.localIP().toString().c_str(),
            System.rssi);

        return true;
    }

    bool startUdp()
    {
        if (udpStarted)
        {
            return true;
        }

        if (!udp.begin(KettleCallPort))
        {
            Logger::error("NETWORK", "Failed to start UDP on port %u", KettleCallPort);
            udpStarted = false;
            return false;
        }

        udpStarted = true;

        Logger::info("NETWORK", "UDP listening on port %u", KettleCallPort);

        return true;
    }

    bool startMdns()
    {
        if (mdnsStarted)
        {
            return true;
        }

        if (!MDNS.begin(mdnsHostName))
        {
            Logger::error("NETWORK", "Failed to start mDNS");
            mdnsStarted = false;
            return false;
        }

        mdnsStarted = true;

        Logger::info("NETWORK", "mDNS started as %s.local", mdnsHostName);

        MDNS.addService(MdnsServiceName, MdnsProtocol, KettleCallPort);

        Logger::info(
            "NETWORK",
            "Advertising service _%s._%s on port %u",
            MdnsServiceName,
            MdnsProtocol,
            KettleCallPort);

        return true;
    }

    bool sendPacket(PacketType type, const void* payload, uint16_t payloadLength)
    {
        if (!System.peerConnected)
        {
            return false;
        }

        if (System.peerIP == IPAddress(0, 0, 0, 0))
        {
            return false;
        }

        if (sizeof(PacketHeader) + payloadLength > MaxPacketSize)
        {
            Logger::error(
                "NETWORK",
                "Packet too large: type=%u length=%u",
                static_cast<uint8_t>(type),
                payloadLength);

            System.badPackets++;
            return false;
        }

        PacketHeader header = {};
        header.magic = KettleCallMagic;
        header.protocolVersion = KettleCallProtocolVersion;
        header.type = type;
        header.payloadLength = payloadLength;
        header.sequence = txSequence++;
        header.timestampMs = millis();

        udp.beginPacket(System.peerIP, KettleCallPort);
        udp.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));

        if (payload != nullptr && payloadLength > 0)
        {
            udp.write(reinterpret_cast<const uint8_t*>(payload), payloadLength);
        }

        int result = udp.endPacket();

        if (result == 1)
        {
            System.txPackets++;
            return true;
        }

        return false;
    }

    void sendHello()
    {
        HelloPayload payload = {};

        copyDeviceName(payload.deviceName, sizeof(payload.deviceName), DEVICE_NAME);

        payload.versionMajor = VersionMajor;
        payload.versionMinor = VersionMinor;
        payload.versionPatch = VersionPatch;
        payload.rssi = currentRssi8();

        sendPacket(PacketType::Hello, &payload, sizeof(payload));
    }

    void sendKeepAlive()
    {
        KeepAlivePayload payload = {};

        copyDeviceName(payload.deviceName, sizeof(payload.deviceName), DEVICE_NAME);

        payload.uptimeSeconds = millis() / 1000;
        payload.rssi = currentRssi8();

        if (sendPacket(PacketType::KeepAlive, &payload, sizeof(payload)))
        {
            System.keepAliveSent++;
        }
    }

    size_t audioPayloadSize(uint16_t sampleCount)
    {
        return offsetof(AudioPayload, samples) + (sampleCount * sizeof(int16_t));
    }

    void processIncomingAudio(const PacketHeader& header, const uint8_t* payload)
    {
        if (payload == nullptr)
        {
            System.audioRxBad++;
            System.badPackets++;
            return;
        }

        if (header.payloadLength < offsetof(AudioPayload, samples))
        {
            System.audioRxBad++;
            System.badPackets++;
            return;
        }

        const AudioPayload* audio = reinterpret_cast<const AudioPayload*>(payload);

        if (audio->sampleCount == 0 || audio->sampleCount > AudioPacketSamples)
        {
            System.audioRxBad++;
            System.badPackets++;
            return;
        }

        size_t expectedSize = audioPayloadSize(audio->sampleCount);

        if (header.payloadLength != expectedSize)
        {
            System.audioRxBad++;
            System.badPackets++;
            return;
        }

        // Speaker playback will be added later.
        // For now we only prove audio UDP packets are arriving cleanly.
        System.audioPacketsReceived++;

        AudioManager::queueReceivedFrame(
            audio->samples,
            audio->sampleCount);
    }

    void processOutgoingAudio()
    {
        if (audioTxQueue == nullptr)
        {
            return;
        }

        if (!System.peerConnected)
        {
            xQueueReset(audioTxQueue);
            return;
        }

        AudioPayload payload = {};

        while (xQueueReceive(audioTxQueue, &payload, 0) == pdTRUE)
        {
            size_t payloadLength = audioPayloadSize(payload.sampleCount);

            bool sent = sendPacket(
                PacketType::Audio,
                &payload,
                static_cast<uint16_t>(payloadLength));

            if (sent)
            {
                System.audioPacketsSent++;
            }
            else
            {
                System.audioTxDropped++;
            }
        }
    }

    void processHelloPacket(IPAddress remoteIP, const uint8_t* payload, uint16_t payloadLength)
    {
        if (payloadLength != sizeof(HelloPayload))
        {
            System.badPackets++;
            return;
        }

        HelloPayload hello = {};
        memcpy(&hello, payload, sizeof(hello));

        updatePeer(remoteIP, hello.deviceName);
    }

    void processKeepAlivePacket(IPAddress remoteIP, const uint8_t* payload, uint16_t payloadLength)
    {
        if (payloadLength != sizeof(KeepAlivePayload))
        {
            System.badPackets++;
            return;
        }

        KeepAlivePayload keepAlive = {};
        memcpy(&keepAlive, payload, sizeof(keepAlive));

        System.keepAliveReceived++;

        updatePeer(remoteIP, keepAlive.deviceName);
    }

    void processIncomingPackets()
    {
        int packetSize = udp.parsePacket();

        while (packetSize > 0)
        {
            IPAddress remoteIP = udp.remoteIP();

            if (packetSize > static_cast<int>(MaxPacketSize))
            {
                Logger::error("NETWORK", "Incoming packet too large: %d", packetSize);

                while (udp.available())
                {
                    udp.read();
                }

                System.badPackets++;
                packetSize = udp.parsePacket();
                continue;
            }

            int bytesRead = udp.read(packetBuffer, MaxPacketSize);

            if (bytesRead < static_cast<int>(sizeof(PacketHeader)))
            {
                System.badPackets++;
                packetSize = udp.parsePacket();
                continue;
            }

            PacketHeader header = {};
            memcpy(&header, packetBuffer, sizeof(header));

            if (header.magic != KettleCallMagic)
            {
                System.badPackets++;
                packetSize = udp.parsePacket();
                continue;
            }

            if (header.protocolVersion != KettleCallProtocolVersion)
            {
                System.badPackets++;
                packetSize = udp.parsePacket();
                continue;
            }

            if (header.payloadLength != bytesRead - sizeof(PacketHeader))
            {
                System.badPackets++;
                packetSize = udp.parsePacket();
                continue;
            }

            const uint8_t* payload = packetBuffer + sizeof(PacketHeader);

            System.rxPackets++;

            switch (header.type)
            {
                case PacketType::Hello:
                    processHelloPacket(remoteIP, payload, header.payloadLength);
                    break;

                case PacketType::KeepAlive:
                    processKeepAlivePacket(remoteIP, payload, header.payloadLength);
                    break;

                case PacketType::Audio:
                    processIncomingAudio(header, payload);
                    break;

                case PacketType::Ack:
                case PacketType::Diagnostics:
                case PacketType::Config:
                case PacketType::Unknown:
                default:
                    System.badPackets++;
                    break;
            }

            packetSize = udp.parsePacket();
        }
    }

    void searchForPeer()
    {
        if (!mdnsStarted)
        {
            return;
        }

        Logger::debug("NETWORK", "Searching for KettleCall peers...");

        int count = MDNS.queryService(MdnsServiceName, MdnsProtocol);

        for (int i = 0; i < count; i++)
        {
            IPAddress ip = MDNS.IP(i);

            if (ip == IPAddress(0, 0, 0, 0))
            {
                continue;
            }

            if (ip == WiFi.localIP())
            {
                continue;
            }

            String host = MDNS.hostname(i);

            char peerName[32] = {0};

            if (host.startsWith("kettlecall-"))
            {
                host.remove(0, strlen("kettlecall-"));
            }

            host.toCharArray(peerName, sizeof(peerName));

            updatePeer(ip, peerName);

            Logger::info(
                "NETWORK",
                "Found peer via mDNS: %s at %s",
                peerName,
                ip.toString().c_str());

            sendHello();

            return;
        }
    }

    void handlePeerTimeout()
    {
        if (!System.peerConnected)
        {
            return;
        }

        uint32_t now = millis();

        if (now - System.lastPeerSeenMs <= PeerTimeoutMs)
        {
            return;
        }

        Logger::error("NETWORK", "Peer timeout: %s", System.peerName);

        System.peerConnected = false;
        System.peerIP = IPAddress(0, 0, 0, 0);
        memset(System.peerName, 0, sizeof(System.peerName));

        if (audioTxQueue != nullptr)
        {
            xQueueReset(audioTxQueue);
        }
    }

    void updateLedState()
    {
        if (System.transmitting)
        {
            System.ledState = LedState::TX;
            LED::update();
        }
        else if (System.peerConnected)
        {
            setLedReady();
        }
        else if (System.wifiConnected)
        {
            setLedSearching();
        }
        else
        {
            setLedError();
        }
    }

    void networkTask(void*)
    {
        Logger::info("NETWORK", "Network task started");

        makeMdnsHostName();

        connectWiFi();
        startUdp();
        startMdns();

        lastPeerSearchMs = millis();
        lastKeepAliveMs = millis();

        while (true)
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                Logger::error("NETWORK", "WiFi disconnected");

                System.wifiConnected = false;
                System.peerConnected = false;
                System.peerIP = IPAddress(0, 0, 0, 0);
                memset(System.peerName, 0, sizeof(System.peerName));

                setLedSearching();

                udpStarted = false;
                mdnsStarted = false;

                connectWiFi();
                startUdp();
                startMdns();
            }

            System.wifiConnected = true;
            System.rssi = WiFi.RSSI();
            System.uptime = millis() / 1000;
            System.freeHeap = ESP.getFreeHeap();

            processIncomingPackets();
            processOutgoingAudio();

            uint32_t now = millis();

            if (!System.peerConnected && now - lastPeerSearchMs >= PeerSearchIntervalMs)
            {
                lastPeerSearchMs = now;
                searchForPeer();
            }

            if (System.peerConnected && now - lastKeepAliveMs >= KeepAliveIntervalMs)
            {
                lastKeepAliveMs = now;
                sendKeepAlive();
            }

            handlePeerTimeout();
            updateLedState();

            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void NetworkManager::begin()
{
    if (audioTxQueue == nullptr)
    {
        audioTxQueue = xQueueCreate(AudioTxQueueLength, sizeof(AudioPayload));

        if (audioTxQueue == nullptr)
        {
            Logger::error("NETWORK", "Failed to create audio TX queue");
        }
        else
        {
            Logger::info("NETWORK", "Audio TX queue ready");
        }
    }

    TaskManager::create(
        "Network",
        networkTask,
        nullptr,
        8192,
        1,
        0);
}

bool NetworkManager::wifiConnected()
{
    return System.wifiConnected;
}

bool NetworkManager::peerConnected()
{
    return System.peerConnected;
}

IPAddress NetworkManager::localIP()
{
    return WiFi.localIP();
}

IPAddress NetworkManager::peerIP()
{
    return System.peerIP;
}

bool NetworkManager::queueAudioFrame(const int16_t* samples, uint16_t sampleCount)
{
    if (audioTxQueue == nullptr || samples == nullptr || sampleCount == 0)
    {
        System.audioTxDropped++;
        return false;
    }

    if (!System.peerConnected)
    {
        System.audioTxDropped++;
        return false;
    }

    if (sampleCount > AudioPacketSamples)
    {
        sampleCount = AudioPacketSamples;
    }

    AudioPayload payload = {};
    payload.sampleCount = sampleCount;
    payload.reserved = 0;

    memcpy(
        payload.samples,
        samples,
        sampleCount * sizeof(int16_t));

    if (xQueueSend(audioTxQueue, &payload, 0) != pdTRUE)
    {
        System.audioTxDropped++;
        return false;
    }

    return true;
}