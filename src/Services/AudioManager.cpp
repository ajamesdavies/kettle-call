#include "Services/AudioManager.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <stdlib.h>
#include <cstring>

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/Packet.h"
#include "Core/SystemState.h"
#include "Core/TaskManager.h"

#include "Services/NetworkManager.h"

extern SystemState System;

namespace
{
    constexpr i2s_port_t MicPort = I2S_NUM_0;
    constexpr i2s_port_t SpeakerPort = I2S_NUM_1;

    constexpr uint8_t AudioRxQueueLength = 8;

    // Start quiet. 16 = one sixteenth volume.
    constexpr int32_t PlaybackVolumeDivisor = 2;
    constexpr int32_t TxGainMultiplier = 4;

    bool audioReady = false;

    QueueHandle_t audioRxQueue = nullptr;

    int32_t rawMicBuffer[AudioFrameSamples];
    int16_t txAudioBuffer[AudioFrameSamples];

    int16_t speakerPlaybackBuffer[AudioPacketSamples];
    int16_t speakerSilenceBuffer[AudioPacketSamples] = {0};

     void setAmpEnabled(bool enabled)
    {
        digitalWrite(I2S_AMP_SD, enabled ? HIGH : LOW);
    }

    struct ReceivedAudioFrame
    {
        uint16_t sampleCount;
        int16_t samples[AudioPacketSamples];
    };

    void updateRxQueueDepth()
    {
        if (audioRxQueue == nullptr)
        {
            System.audioRxQueueDepth = 0;
            return;
        }

        System.audioRxQueueDepth = static_cast<uint32_t>(
            uxQueueMessagesWaiting(audioRxQueue));
    }

    int32_t calculatePcm16Peak(const int16_t* samples, size_t sampleCount)
    {
        int32_t peak = 0;

        if (samples == nullptr)
        {
            return 0;
        }

        for (size_t i = 0; i < sampleCount; i++)
        {
            int32_t sample = samples[i];

            if (sample < 0)
            {
                sample = -sample;
            }

            if (sample > peak)
            {
                peak = sample;
            }
        }

        return peak;
    }

    bool setupMicrophone()
    {
        i2s_config_t config = {};
        config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
        config.sample_rate = AudioSampleRate;
        config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
        config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
        config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
        config.dma_buf_count = 8;
        config.dma_buf_len = 128;
        config.use_apll = false;
        config.tx_desc_auto_clear = false;
        config.fixed_mclk = 0;

        i2s_pin_config_t pins = {};
        pins.bck_io_num = I2S_MIC_SCK;
        pins.ws_io_num = I2S_MIC_WS;
        pins.data_out_num = I2S_PIN_NO_CHANGE;
        pins.data_in_num = I2S_MIC_SD;

        esp_err_t result = i2s_driver_install(MicPort, &config, 0, nullptr);

        if (result != ESP_OK)
        {
            Logger::error("AUDIO", "Mic driver install failed: %d", result);
            return false;
        }

        result = i2s_set_pin(MicPort, &pins);

        if (result != ESP_OK)
        {
            Logger::error("AUDIO", "Mic pin setup failed: %d", result);
            return false;
        }

        i2s_zero_dma_buffer(MicPort);

        Logger::info(
            "AUDIO",
            "Mic ready: SCK=%d WS=%d SD=%d Rate=%u",
            I2S_MIC_SCK,
            I2S_MIC_WS,
            I2S_MIC_SD,
            AudioSampleRate);

        return true;
    }

    bool setupSpeaker()
    {
        i2s_config_t config = {};
        config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
        config.sample_rate = AudioSampleRate;
        config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
        config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
        config.dma_buf_count = 8;
        config.dma_buf_len = 128;
        config.use_apll = false;
        config.tx_desc_auto_clear = true;
        config.fixed_mclk = 0;

        i2s_pin_config_t pins = {};
        pins.bck_io_num = I2S_SPK_BCLK;
        pins.ws_io_num = I2S_SPK_LRC;
        pins.data_out_num = I2S_SPK_DIN;
        pins.data_in_num = I2S_PIN_NO_CHANGE;

        esp_err_t result = i2s_driver_install(SpeakerPort, &config, 0, nullptr);

        if (result != ESP_OK)
        {
            Logger::error("AUDIO", "Speaker driver install failed: %d", result);
            return false;
        }

        result = i2s_set_pin(SpeakerPort, &pins);

        if (result != ESP_OK)
        {
            Logger::error("AUDIO", "Speaker pin setup failed: %d", result);
            return false;
        }

        i2s_zero_dma_buffer(SpeakerPort);

        Logger::info(
            "AUDIO",
            "Speaker ready: BCLK=%d LRC=%d DIN=%d Rate=%u",
            I2S_SPK_BCLK,
            I2S_SPK_LRC,
            I2S_SPK_DIN,
            AudioSampleRate);

        return true;
    }

    int32_t calculatePeak(const int32_t* samples, size_t sampleCount)
    {
        int32_t peak = 0;

        for (size_t i = 0; i < sampleCount; i++)
        {
            int32_t sample = samples[i] >> 12;
            int32_t magnitude = abs(sample);

            if (magnitude > peak)
            {
                peak = magnitude;
            }
        }

        return peak;
    }

    int16_t convertSampleToPcm16(int32_t rawSample)
    {
        int32_t sample = rawSample >> 16;
        sample *= TxGainMultiplier;
        if (sample > 32767)
        {
            sample = 32767;
        }
        else if (sample < -32768)
        {
            sample = -32768;
        }

        return static_cast<int16_t>(sample);
    }

    void convertFrameToPcm16(
        const int32_t* input,
        int16_t* output,
        size_t sampleCount)
    {
        for (size_t i = 0; i < sampleCount; i++)
        {
            output[i] = convertSampleToPcm16(input[i]);
        }
    }

    void prepareSpeakerFrame(
        const ReceivedAudioFrame& frame,
        int16_t* output)
    {
        uint16_t sampleCount = frame.sampleCount;

        if (sampleCount > AudioPacketSamples)
        {
            sampleCount = AudioPacketSamples;
        }

        for (uint16_t i = 0; i < sampleCount; i++)
        {
            int32_t sample = frame.samples[i];
            sample /= PlaybackVolumeDivisor;

            if (sample > 32767)
            {
                sample = 32767;
            }
            else if (sample < -32768)
            {
                sample = -32768;
            }

            output[i] = static_cast<int16_t>(sample);
        }

        for (uint16_t i = sampleCount; i < AudioPacketSamples; i++)
        {
            output[i] = 0;
        }

        System.speakerAudioPeak = calculatePcm16Peak(
            output,
            sampleCount);
    }

    bool writeSpeakerSamples(const int16_t* samples, uint16_t sampleCount)
    {
        if (samples == nullptr || sampleCount == 0)
        {
            return false;
        }

        size_t bytesToWrite = sampleCount * sizeof(int16_t);
        size_t bytesWritten = 0;

        esp_err_t result = i2s_write(
            SpeakerPort,
            samples,
            bytesToWrite,
            &bytesWritten,
            pdMS_TO_TICKS(50));

        if (result != ESP_OK || bytesWritten != bytesToWrite)
        {
            System.speakerWriteErrors++;
            return false;
        }

        return true;
    }

    void writeSpeakerSilence()
    {
        if (writeSpeakerSamples(speakerSilenceBuffer, AudioPacketSamples))
        {
            System.speakerSilenceFrames++;
        }
    }

    void audioCaptureTask(void*)
    {
        Logger::info("AUDIO", "Mic capture task started");

        while (true)
        {
            size_t bytesRead = 0;

            esp_err_t result = i2s_read(
                MicPort,
                rawMicBuffer,
                sizeof(rawMicBuffer),
                &bytesRead,
                portMAX_DELAY);

            if (result != ESP_OK || bytesRead == 0)
            {
                System.micReadErrors++;
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            size_t samplesRead = bytesRead / sizeof(int32_t);

            if (samplesRead > AudioFrameSamples)
            {
                samplesRead = AudioFrameSamples;
            }

            int32_t peak = calculatePeak(rawMicBuffer, samplesRead);

            if (peak > System.micPeak)
            {
                System.micPeak = peak;
            }
            else
            {
                System.micPeak = (System.micPeak * 9) / 10;
            }

            System.micFrames++;

            if (System.transmitting && System.peerConnected)
            {
                convertFrameToPcm16(
                    rawMicBuffer,
                    txAudioBuffer,
                    samplesRead);

                System.txAudioPeak = calculatePcm16Peak(
                    txAudioBuffer,
                    samplesRead);

                NetworkManager::queueAudioFrame(
                    txAudioBuffer,
                    static_cast<uint16_t>(samplesRead));
            }
        }
    }

    void audioPlaybackTask(void*)
    {
        Logger::info(
            "AUDIO",
            "Speaker playback task started at 1/%ld volume",
            PlaybackVolumeDivisor);

        ReceivedAudioFrame frame = {};

        while (true)
        {
            if (audioRxQueue == nullptr)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (System.transmitting)
            {
            setAmpEnabled(false);
            xQueueReset(audioRxQueue);
            updateRxQueueDepth();
            System.receiving = false;
            writeSpeakerSilence();
            continue;
            }
            else
            {
                setAmpEnabled(true);
            }

            if (xQueueReceive(audioRxQueue, &frame, pdMS_TO_TICKS(5)) == pdTRUE)
            {
                prepareSpeakerFrame(frame, speakerPlaybackBuffer);

                System.audioRxDequeued++;
                updateRxQueueDepth();

                if (writeSpeakerSamples(speakerPlaybackBuffer, frame.sampleCount))
                {
                    System.speakerFramesWritten++;
                    System.receiving = true;
                }
                else
                {
                    System.receiving = false;
                }
            }
            else
            {
                updateRxQueueDepth();
                System.receiving = false;
                writeSpeakerSilence();
            }
        }
    }
}

void AudioManager::begin()
{
    Logger::info("AUDIO", "Starting audio manager");

    pinMode(I2S_AMP_SD, OUTPUT);
    setAmpEnabled(false);
    Logger::info("AUDIO", "Amp SD pin ready on GPIO%d", I2S_AMP_SD);

    if (audioRxQueue == nullptr)
    {
        audioRxQueue = xQueueCreate(
            AudioRxQueueLength,
            sizeof(ReceivedAudioFrame));

        if (audioRxQueue == nullptr)
        {
            Logger::error("AUDIO", "Failed to create RX audio queue");
        }
        else
        {
            Logger::info("AUDIO", "RX audio queue ready");
        }
    }

    bool micOk = setupMicrophone();
    bool speakerOk = setupSpeaker();

    audioReady = micOk && speakerOk;

    if (audioReady)
    {
        Logger::info("AUDIO", "Audio manager ready");
        setAmpEnabled(true);

        bool captureCreated = TaskManager::create(
            "AudioCapture",
            audioCaptureTask,
            nullptr,
            4096,
            2,
            1);

        if (!captureCreated)
        {
            Logger::error("AUDIO", "Failed to create mic capture task");
            audioReady = false;
        }

        if (audioRxQueue != nullptr)
        {
            bool playbackCreated = TaskManager::create(
                "AudioPlayback",
                audioPlaybackTask,
                nullptr,
                4096,
                2,
                1);

            if (!playbackCreated)
            {
                Logger::error("AUDIO", "Failed to create speaker playback task");
            }
        }
    }
    else
    {
        Logger::error("AUDIO", "Audio manager failed to start");
    }
}

bool AudioManager::ready()
{
    return audioReady;
}

bool AudioManager::queueReceivedFrame(const int16_t* samples, uint16_t sampleCount)
{
    if (audioRxQueue == nullptr || samples == nullptr || sampleCount == 0)
    {
        System.audioRxQueueDropped++;
        updateRxQueueDepth();
        return false;
    }

    if (System.transmitting)
    {
        updateRxQueueDepth();
        return true;
    }

    if (sampleCount > AudioPacketSamples)
    {
        sampleCount = AudioPacketSamples;
    }

    ReceivedAudioFrame frame = {};
    frame.sampleCount = sampleCount;

    System.rxAudioPeak = calculatePcm16Peak(
        samples,
        sampleCount);

    memcpy(
        frame.samples,
        samples,
        sampleCount * sizeof(int16_t));

    if (xQueueSend(audioRxQueue, &frame, 0) != pdTRUE)
    {
        System.audioRxQueueDropped++;
        updateRxQueueDepth();
        return false;
    }

    System.audioRxQueued++;
    updateRxQueueDepth();

    return true;
}