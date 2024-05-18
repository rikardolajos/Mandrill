#include "Log.h"

using namespace Mandrill;

inline std::string getLevelString(Log::Level level)
{
    switch (level) {
    case Log::Level::Info:
        return "";
    case Log::Level::Debug:
        return "\x1B[1;92mDEBUG: \x1B[0m";
    case Log::Level::Warning:
        return "\x1B[1;93mWARNING: \x1B[0m";
    case Log::Level::Error:
        return "\x1B[1;91mERROR: \x1B[0m";
    default:
        return "";
    }
}

void Log::log(Level level, const std::string msg)
{
    std::string s = std::format("{}{}\n", getLevelString(level), msg);
    auto& os = level < Log::Level::Error ? std::cout : std::cerr;
    os << s;
    os.flush();
}
