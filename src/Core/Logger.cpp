#include "Core/Logger.h"

#include <stdarg.h>

namespace
{
    const char *levelString(Logger::Level level)
    {
        switch (level)
        {
            case Logger::Level::Info:
                return "INFO";

            case Logger::Level::Warning:
                return "WARN";

            case Logger::Level::Error:
                return "ERROR";

            case Logger::Level::Debug:
                return "DEBUG";

            default:
                return "????";
        }
    }

    void print(Logger::Level level,
               const char *tag,
               const char *format,
               va_list args)
    {
        char message[256];

        vsnprintf(message,
                  sizeof(message),
                  format,
                  args);

        Serial.printf("[%10lu] %-5s %-12s %s\n",
                      millis(),
                      levelString(level),
                      tag,
                      message);
    }
}

void Logger::begin()
{
}

void Logger::log(Level level,
                 const char *tag,
                 const char *format,
                 ...)
{
    va_list args;

    va_start(args, format);

    print(level,
          tag,
          format,
          args);

    va_end(args);
}

void Logger::info(const char *tag,
                  const char *format,
                  ...)
{
    va_list args;

    va_start(args, format);

    print(Level::Info,
          tag,
          format,
          args);

    va_end(args);
}

void Logger::warn(const char *tag,
                  const char *format,
                  ...)
{
    va_list args;

    va_start(args, format);

    print(Level::Warning,
          tag,
          format,
          args);

    va_end(args);
}

void Logger::error(const char *tag,
                   const char *format,
                   ...)
{
    va_list args;

    va_start(args, format);

    print(Level::Error,
          tag,
          format,
          args);

    va_end(args);
}

void Logger::debug(const char *tag,
                   const char *format,
                   ...)
{
    va_list args;

    va_start(args, format);

    print(Level::Debug,
          tag,
          format,
          args);

    va_end(args);
}