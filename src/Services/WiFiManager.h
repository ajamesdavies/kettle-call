#pragma once

#include <Arduino.h>

namespace WiFiManager
{
    void begin();

    bool connected();

    int rssi();

    String ipAddress();
}