#pragma once

#include <Arduino.h>

namespace TaskManager
{
    bool create(
        const char* name,
        TaskFunction_t task,
        void* parameter,
        uint32_t stackSize,
        UBaseType_t priority,
        BaseType_t core);
}