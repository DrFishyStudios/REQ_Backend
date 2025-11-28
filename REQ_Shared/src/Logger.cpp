#include "../include/req/shared/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace req::shared {

namespace {
    std::string g_appName;
    std::mutex g_logMutex;

    std::string timestamp() {
        using namespace std::chrono;
        const auto now = system_clock::now();
        std::time_t tt = system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    void write(std::ostream& os, const char* level, const std::string& category, const std::string& message) {
        std::scoped_lock lock(g_logMutex);
        os << '[' << timestamp() << "] [" << (g_appName.empty() ? "REQ" : g_appName) << "] [" << level << "] [" << category << "] " << message << '\n';
    }
}

void initLogger(const std::string& appName) {
    std::scoped_lock lock(g_logMutex);
    g_appName = appName;
}

void logInfo(const std::string& category, const std::string& message) {
    write(std::cout, "INFO", category, message);
}

void logWarn(const std::string& category, const std::string& message) {
    write(std::cout, "WARN", category, message);
}

void logError(const std::string& category, const std::string& message) {
    write(std::cerr, "ERROR", category, message);
}

} // namespace req::shared
