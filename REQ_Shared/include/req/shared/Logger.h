#pragma once

#include <string>

namespace req::shared {

void initLogger(const std::string& appName);
void logInfo(const std::string& category, const std::string& message);
void logWarn(const std::string& category, const std::string& message);
void logError(const std::string& category, const std::string& message);

} // namespace req::shared
