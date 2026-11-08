#pragma once

#include <Arduino.h>

namespace Logger
{
    enum class Level
    {
        Info,
        Warning,
        Error,
        Debug
    };

    void begin();

    void log(Level level, const char *tag, const char *format, ...);

    void info(const char *tag, const char *format, ...);

    void warn(const char *tag, const char *format, ...);

    void error(const char *tag, const char *format, ...);

    void debug(const char *tag, const char *format, ...);
}