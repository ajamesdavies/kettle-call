#include "Core/SystemManager.h"

SystemManager::SystemManager()
{
    mutex = xSemaphoreCreateMutex();
}

SystemManager& SystemManager::instance()
{
    static SystemManager manager;
    return manager;
}

void SystemManager::begin()
{
}

SystemState SystemManager::getState()
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    SystemState copy = state;

    xSemaphoreGive(mutex);

    return copy;
}

void SystemManager::setLedState(LedState ledState)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    state.ledState = ledState;

    xSemaphoreGive(mutex);
}

LedState SystemManager::getLedState()
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    LedState led = state.ledState;

    xSemaphoreGive(mutex);

    return led;
}