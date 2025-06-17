#pragma once

#include "Common.h"

namespace Mandrill
{
    /// <summary>
    /// Log class for logging messages in Mandrill.
    /// </summary>
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

        /// <summary>
        /// Log an information message using format strings.
        /// </summary>
        /// <typeparam name="...Args"></typeparam>
        /// <param name="format">Format string</param>
        /// <param name="...args">Format string arguments</param>
        template <typename... Args> inline static void Info(std::format_string<Args...> format, Args&&... args)
        {
            log(Level::Info, std::format(format, std::forward<Args>(args)...));
        }

        /// <summary>
        /// Log a debug message using format strings. This does not print message in release mode.
        /// </summary>
        /// <typeparam name="...Args"></typeparam>
        /// <param name="format">Format string</param>
        /// <param name="...args">Format string arguments</param>
        template <typename... Args> inline static void Debug(std::format_string<Args...> format, Args&&... args)
        {
#ifdef _DEBUG
            log(Level::Debug, std::format(format, std::forward<Args>(args)...));
#endif
        }

        /// <summary>
        /// Log a warning message using format strings.
        /// </summary>
        /// <typeparam name="...Args"></typeparam>
        /// <param name="format">Format string</param>
        /// <param name="...args">Format string arguments</param>
        template <typename... Args> inline static void Warning(std::format_string<Args...> format, Args&&... args)
        {
            log(Level::Warning, std::format(format, std::forward<Args>(args)...));
        }

        /// <summary>
        /// Log an error message using format strings.
        /// </summary>
        /// <typeparam name="...Args"></typeparam>
        /// <param name="format">Format string</param>
        /// <param name="...args">Format string arguments</param>
        template <typename... Args> inline static void Error(std::format_string<Args...> format, Args&&... args)
        {
            log(Level::Error, std::format(format, std::forward<Args>(args)...));
        }
    };

} // namespace Mandrill
