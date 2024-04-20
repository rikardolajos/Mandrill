#include "Log.h"

using namespace Mandrill;

inline std::string getLevelString(Log::Level level)
{
    switch (level) {
    case Log::Level::Info:
        return "(Info)";
    case Log::Level::Debug:
        return "(Debug)";
    case Log::Level::Warning:
        return "(Warning)";
    case Log::Level::Error:
        return "(Error)";
    default:
        return "";
    }
}

void Log::log(Level level, const std::string msg)
{
    std::string s = std::format("{} {}\n", getLevelString(level), msg);
}
