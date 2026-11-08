#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "Core/SystemState.h"

class SystemManager
{
public:

    static SystemManager& instance();

    void begin();

    SystemState getState();

    void setLedState(LedState state);

    LedState getLedState();

private:

    SystemManager();

    SemaphoreHandle_t mutex;

    SystemState state;
};