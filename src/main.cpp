#include <Arduino.h>

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/SystemState.h"

#include "Drivers/LED.h"

#include "Services/NetworkManager.h"
#include "Services/PttManager.h"
#include "Services/AudioManager.h"
#include "Services/Diagnostics.h"

SystemState System;

void setup()
{
    Serial.begin(115200);

    // Give USB serial a short moment to connect.
    vTaskDelay(pdMS_TO_TICKS(500));

    Logger::begin();

    LED::begin();

    System.ledState = LedState::Boot;
    LED::update();

    Logger::info("SYSTEM", "=======================================");
    Logger::info("SYSTEM", "Project KettleCall");

    Logger::info(
        "SYSTEM",
        "Firmware %u.%u.%u",
        VersionMajor,
        VersionMinor,
        VersionPatch);

    Logger::info("SYSTEM", "Device %s", DEVICE_NAME);
    Logger::info("SYSTEM", "Booting...");
    Logger::info("SYSTEM", "=======================================");

    NetworkManager::begin();
    PttManager::begin();
    AudioManager::begin();
    Diagnostics::begin();
}

void loop()
{
    // Everything runs inside FreeRTOS tasks.
    vTaskDelay(portMAX_DELAY);
}