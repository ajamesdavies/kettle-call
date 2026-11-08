#include "Core/TaskManager.h"

bool TaskManager::create(
    const char* name,
    TaskFunction_t task,
    void* parameter,
    uint32_t stackSize,
    UBaseType_t priority,
    BaseType_t core)
{
    TaskHandle_t handle = nullptr;

    BaseType_t result = xTaskCreatePinnedToCore(
        task,
        name,
        stackSize,
        parameter,
        priority,
        &handle,
        core);

    return result == pdPASS;
}