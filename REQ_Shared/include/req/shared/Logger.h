#pragma once

#include <string>

/*
 * REQ Backend Logging System
 * 
 * Standard Log Format:
 *   [YYYY-MM-DD HH:MM:SS] [ExecutableName] [LEVEL] [category] message
 * 
 * Example:
 *   [2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginRequest: username=Rich, mode=login
 * 
 * Usage:
 *   1. Call initLogger("REQ_<ServerName>") in main()
 *   2. Use logInfo/logWarn/logError throughout the application
 *   3. Choose meaningful category names (e.g., "login", "world", "zone", "Main")
 * 
 * Guidelines:
 *   - Log all request/response messages with key fields
 *   - Log all validation failures and error paths
 *   - Include enough context to trace a single client's journey
 *   - Use consistent field naming (e.g., sessionToken, characterId, worldId)
 */

namespace req::shared {

void initLogger(const std::string& appName);
void logInfo(const std::string& category, const std::string& message);
void logWarn(const std::string& category, const std::string& message);
void logError(const std::string& category, const std::string& message);

} // namespace req::shared
