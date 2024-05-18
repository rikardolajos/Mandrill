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


        template <typename... Args> inline static void info(std::format_string<Args...> format, Args&&... args)
        {
            log(Level::Info, std::format(format, std::forward<Args>(args)...));
        }

        template <typename... Args> inline static void debug(std::format_string<Args...> format, Args&&... args)
        {
#ifdef _DEBUG
            log(Level::Debug, std::format(format, std::forward<Args>(args)...));
#endif
        }

        template <typename... Args> inline static void warning(std::format_string<Args...> format, Args&&... args)
        {
            log(Level::Warning, std::format(format, std::forward<Args>(args)...));
        }

        template <typename... Args> inline static void error(std::format_string<Args...> format, Args&&... args)
        {
            log(Level::Error, std::format(format, std::forward<Args>(args)...));
        }
    };

} // namespace Mandrill