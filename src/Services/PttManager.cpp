#include "Services/PttManager.h"

#include <Arduino.h>

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/SystemState.h"
#include "Core/TaskManager.h"

extern SystemState System;

namespace
{
    constexpr uint32_t PollIntervalMs = 10;
    constexpr uint32_t DebounceMs = 40;

    bool stablePressed = false;
    bool lastRawPressed = false;
    uint32_t lastRawChangeMs = 0;

    bool readButtonPressed()
    {
        return digitalRead(PTT_PIN) == LOW;
    }

    void pttTask(void*)
    {
        pinMode(PTT_PIN, INPUT_PULLUP);

        lastRawPressed = readButtonPressed();
        stablePressed = lastRawPressed;
        lastRawChangeMs = millis();

        System.transmitting = stablePressed;

        Logger::info("PTT", "PTT task started on GPIO%u", PTT_PIN);

        while (true)
        {
            bool rawPressed = readButtonPressed();
            uint32_t now = millis();

            if (rawPressed != lastRawPressed)
            {
                lastRawPressed = rawPressed;
                lastRawChangeMs = now;
            }

            if ((now - lastRawChangeMs) >= DebounceMs)
            {
                if (stablePressed != rawPressed)
                {
                    stablePressed = rawPressed;

                    System.transmitting = stablePressed;
                    System.receiving = false;

                    if (stablePressed)
                    {
                        Logger::info("PTT", "Pressed - transmit enabled");
                    }
                    else
                    {
                        Logger::info("PTT", "Released - transmit disabled");
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(PollIntervalMs));
        }
    }
}

void PttManager::begin()
{
    bool created = TaskManager::create(
        "PTT",
        pttTask,
        nullptr,
        2048,
        2,
        1);

    if (!created)
    {
        Logger::error("PTT", "Failed to create PTT task");
    }
}

bool PttManager::pressed()
{
    return System.transmitting;
}