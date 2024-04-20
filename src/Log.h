#pragma once

#include "Common.h"

namespace Mandrill
{
    class MANDRILL_API Log
    {
    public:
        enum class Level {
            Info,
            Debug,
            Warning,
            Error,
            Count,
        };

        static void log(Level level, const std::string msg);
    };

    template <typename... Args> inline void logInfo(std::format_string<Args...> format, Args&&... args)
    {
        Log::log(Log::Level::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args> inline void logDebug(std::format_string<Args...> format, Args&&... args)
    {
        Log::log(Log::Level::Debug, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args> inline void logWarning(std::format_string<Args...> format, Args&&... args)
    {
        Log::log(Log::Level::Warning, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args> inline void logError(std::format_string<Args...> format, Args&&... args)
    {
        Log::log(Log::Level::Error, std::format(format, std::forward<Args>(args)...));
    }

} // namespace Mandrill