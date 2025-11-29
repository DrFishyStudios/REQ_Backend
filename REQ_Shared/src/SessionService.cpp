#include "../include/req/shared/SessionService.h"
#include "../include/req/shared/Logger.h"

#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace req::shared {

namespace {
    // Helper: Convert time_point to ISO 8601 string
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
        std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
        return oss.str();
    }
    
    // Helper: Parse ISO 8601 string to time_point (basic implementation)
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) {
        // Basic parsing - expects "YYYY-MM-DDTHH:MM:SS"
        std::tm tm{};
        std::istringstream ss(str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        
        if (ss.fail()) {
            // Return epoch on parse failure
            return std::chrono::system_clock::time_point{};
        }
        
        std::time_t tt = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(tt);
    }
}

SessionService& SessionService::instance() {
    static SessionService instance;
    return instance;
}

std::uint64_t SessionService::generateSessionToken() {
    // Initialize RNG on first use
    static bool initialized = false;
    if (!initialized) {
        std::random_device rd;
        rng_.seed(rd());
        initialized = true;
    }
    
    std::uniform_int_distribution<std::uint64_t> dist(1, std::numeric_limits<std::uint64_t>::max());
    
    std::uint64_t token = 0;
    do {
        token = dist(rng_);
    } while (token == 0 || sessions_.count(token) != 0); // Ensure unique and non-zero
    
    return token;
}

std::uint64_t SessionService::createSession(std::uint64_t accountId) {
    std::scoped_lock lock(mutex_);
    
    auto token = generateSessionToken();
    auto now = std::chrono::system_clock::now();
    
    SessionRecord record;
    record.sessionToken = token;
    record.accountId = accountId;
    record.createdAt = now;
    record.lastSeen = now;
    record.boundWorldId = -1; // Not bound to any world yet
    
    sessions_[token] = record;
    
    logInfo("SessionService", std::string{"Session created: accountId="} + 
        std::to_string(accountId) + ", sessionToken=" + std::to_string(token));
    
    return token;
}

std::optional<SessionRecord> SessionService::validateSession(std::uint64_t sessionToken) {
    std::scoped_lock lock(mutex_);
    
    auto it = sessions_.find(sessionToken);
    if (it == sessions_.end()) {
        logWarn("SessionService", std::string{"Session validation failed: sessionToken="} + 
            std::to_string(sessionToken) + " not found");
        return std::nullopt;
    }
    
    // Update lastSeen
    auto now = std::chrono::system_clock::now();
    it->second.lastSeen = now;
    
    logInfo("SessionService", std::string{"Session validated: sessionToken="} + 
        std::to_string(sessionToken) + ", accountId=" + std::to_string(it->second.accountId) + 
        ", boundWorldId=" + std::to_string(it->second.boundWorldId));
    
    return it->second;
}

void SessionService::bindSessionToWorld(std::uint64_t sessionToken, std::int32_t worldId) {
    std::scoped_lock lock(mutex_);
    
    auto it = sessions_.find(sessionToken);
    if (it == sessions_.end()) {
        logWarn("SessionService", std::string{"Cannot bind session to world: sessionToken="} + 
            std::to_string(sessionToken) + " not found");
        return;
    }
    
    it->second.boundWorldId = worldId;
    
    logInfo("SessionService", std::string{"Session bound to world: sessionToken="} + 
        std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId) + 
        ", accountId=" + std::to_string(it->second.accountId));
}

void SessionService::removeSession(std::uint64_t sessionToken) {
    std::scoped_lock lock(mutex_);
    
    auto it = sessions_.find(sessionToken);
    if (it == sessions_.end()) {
        logWarn("SessionService", std::string{"Cannot remove session: sessionToken="} + 
            std::to_string(sessionToken) + " not found");
        return;
    }
    
    std::uint64_t accountId = it->second.accountId;
    sessions_.erase(it);
    
    logInfo("SessionService", std::string{"Session removed: sessionToken="} + 
        std::to_string(sessionToken) + ", accountId=" + std::to_string(accountId));
}

std::size_t SessionService::getSessionCount() const {
    std::scoped_lock lock(mutex_);
    return sessions_.size();
}

void SessionService::clearAllSessions() {
    std::scoped_lock lock(mutex_);
    
    std::size_t count = sessions_.size();
    sessions_.clear();
    
    logInfo("SessionService", std::string{"All sessions cleared: count="} + std::to_string(count));
}

void SessionService::configure(const std::string& filePath) {
    std::scoped_lock lock(mutex_);
    
    sessionsFilePath_ = filePath;
    configured_ = true;
    
    logInfo("SessionService", std::string{"Configuring SessionService with file: "} + filePath);
    
    // Try to load existing sessions from file
    // Unlock mutex temporarily to call loadFromFile(path) which will re-lock
    mutex_.unlock();
    bool loaded = loadFromFile(filePath);
    mutex_.lock();
    
    if (loaded) {
        logInfo("SessionService", std::string{"Configured with file '"} + filePath + 
            "', loaded " + std::to_string(sessions_.size()) + " session(s)");
    } else {
        logInfo("SessionService", std::string{"Configured with file '"} + filePath + 
            "' (no existing sessions or file not found)");
    }
}

bool SessionService::isConfigured() const {
    std::scoped_lock lock(mutex_);
    return configured_;
}

bool SessionService::saveToFile() {
    std::scoped_lock lock(mutex_);
    
    if (!configured_) {
        logWarn("SessionService", "Cannot save: SessionService not configured with file path");
        return false;
    }
    
    // Unlock and call the path-based saveToFile
    std::string path = sessionsFilePath_;
    mutex_.unlock();
    bool result = saveToFile(path);
    mutex_.lock();
    
    return result;
}

bool SessionService::loadFromFile() {
    std::scoped_lock lock(mutex_);
    
    if (!configured_) {
        logWarn("SessionService", "Cannot load: SessionService not configured with file path");
        return false;
    }
    
    // Unlock and call the path-based loadFromFile
    std::string path = sessionsFilePath_;
    mutex_.unlock();
    bool result = loadFromFile(path);
    mutex_.lock();
    
    return result;
}

bool SessionService::loadFromFile(const std::string& path) {
    std::scoped_lock lock(mutex_);
    
    std::ifstream file(path);
    if (!file.is_open()) {
        // File doesn't exist - not an error, just means no sessions yet
        logInfo("SessionService", std::string{"Session file not found (will be created on first save): "} + path);
        return true;
    }
    
    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    if (content.empty()) {
        logInfo("SessionService", std::string{"Session file is empty: "} + path);
        return true; // Not an error, just no sessions to load
    }
    
    // Basic JSON parsing (simplified - assumes well-formed JSON)
    // Format: {"sessions":[{"sessionToken":123,"accountId":456,"createdAt":"...","lastSeen":"...","boundWorldId":1},...]}
    
    // Clear existing sessions
    sessions_.clear();
    
    // Find "sessions" array
    std::size_t sessionsStart = content.find("\"sessions\"");
    if (sessionsStart == std::string::npos) {
        logError("SessionService", "Invalid session file format: missing 'sessions' array");
        return false;
    }
    
    // Find array start
    std::size_t arrayStart = content.find('[', sessionsStart);
    if (arrayStart == std::string::npos) {
        logError("SessionService", "Invalid session file format: missing array start");
        return false;
    }
    
    // Find array end
    std::size_t arrayEnd = content.find(']', arrayStart);
    if (arrayEnd == std::string::npos) {
        logError("SessionService", "Invalid session file format: missing array end");
        return false;
    }
    
    // Parse each session object
    std::size_t pos = arrayStart + 1;
    int loadedCount = 0;
    
    while (pos < arrayEnd) {
        // Find next object start
        std::size_t objStart = content.find('{', pos);
        if (objStart >= arrayEnd) {
            break;
        }
        
        // Find object end
        std::size_t objEnd = content.find('}', objStart);
        if (objEnd >= arrayEnd) {
            logError("SessionService", "Invalid session file format: unclosed session object");
            return false;
        }
        
        // Extract object content
        std::string objContent = content.substr(objStart, objEnd - objStart + 1);
        
        // Parse fields (basic string parsing)
        SessionRecord record;
        
        // Parse sessionToken
        std::size_t tokenPos = objContent.find("\"sessionToken\"");
        if (tokenPos != std::string::npos) {
            std::size_t colonPos = objContent.find(':', tokenPos);
            std::size_t commaPos = objContent.find(',', colonPos);
            if (commaPos == std::string::npos) commaPos = objContent.find('}', colonPos);
            std::string tokenStr = objContent.substr(colonPos + 1, commaPos - colonPos - 1);
            record.sessionToken = std::stoull(tokenStr);
        }
        
        // Parse accountId
        std::size_t accountPos = objContent.find("\"accountId\"");
        if (accountPos != std::string::npos) {
            std::size_t colonPos = objContent.find(':', accountPos);
            std::size_t commaPos = objContent.find(',', colonPos);
            if (commaPos == std::string::npos) commaPos = objContent.find('}', colonPos);
            std::string accountStr = objContent.substr(colonPos + 1, commaPos - colonPos - 1);
            record.accountId = std::stoull(accountStr);
        }
        
        // Parse createdAt
        std::size_t createdPos = objContent.find("\"createdAt\"");
        if (createdPos != std::string::npos) {
            std::size_t colonPos = objContent.find(':', createdPos);
            std::size_t quoteStart = objContent.find('"', colonPos);
            std::size_t quoteEnd = objContent.find('"', quoteStart + 1);
            std::string createdStr = objContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            record.createdAt = stringToTimePoint(createdStr);
        }
        
        // Parse lastSeen
        std::size_t lastSeenPos = objContent.find("\"lastSeen\"");
        if (lastSeenPos != std::string::npos) {
            std::size_t colonPos = objContent.find(':', lastSeenPos);
            std::size_t quoteStart = objContent.find('"', colonPos);
            std::size_t quoteEnd = objContent.find('"', quoteStart + 1);
            std::string lastSeenStr = objContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            record.lastSeen = stringToTimePoint(lastSeenStr);
        }
        
        // Parse boundWorldId
        std::size_t worldPos = objContent.find("\"boundWorldId\"");
        if (worldPos != std::string::npos) {
            std::size_t colonPos = objContent.find(':', worldPos);
            std::size_t commaPos = objContent.find(',', colonPos);
            if (commaPos == std::string::npos) commaPos = objContent.find('}', colonPos);
            std::string worldStr = objContent.substr(colonPos + 1, commaPos - colonPos - 1);
            record.boundWorldId = std::stoi(worldStr);
        }
        
        // Add to sessions
        if (record.sessionToken != 0 && record.accountId != 0) {
            sessions_[record.sessionToken] = record;
            loadedCount++;
        }
        
        pos = objEnd + 1;
    }
    
    logInfo("SessionService", std::string{"Sessions loaded from file: path="} + path + 
        ", count=" + std::to_string(loadedCount));
    
    return true;
}

bool SessionService::saveToFile(const std::string& path) const {
    std::scoped_lock lock(mutex_);
    
    // Create directory if it doesn't exist
    std::size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string directory = path.substr(0, lastSlash);
        
        // Try to create directory (platform-specific)
#ifdef _WIN32
        std::string cmd = "if not exist \"" + directory + "\" mkdir \"" + directory + "\"";
        system(cmd.c_str());
#else
        std::string cmd = "mkdir -p \"" + directory + "\"";
        system(cmd.c_str());
#endif
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        logError("SessionService", std::string{"Failed to open session file for writing: "} + path);
        return false;
    }
    
    // Write JSON
    file << "{\n";
    file << "  \"sessions\": [\n";
    
    bool first = true;
    for (const auto& [token, record] : sessions_) {
        if (!first) {
            file << ",\n";
        }
        first = false;
        
        file << "    {\n";
        file << "      \"sessionToken\": " << record.sessionToken << ",\n";
        file << "      \"accountId\": " << record.accountId << ",\n";
        file << "      \"createdAt\": \"" << timePointToString(record.createdAt) << "\",\n";
        file << "      \"lastSeen\": \"" << timePointToString(record.lastSeen) << "\",\n";
        file << "      \"boundWorldId\": " << record.boundWorldId << "\n";
        file << "    }";
    }
    
    file << "\n  ]\n";
    file << "}\n";
    
    file.close();
    
    logInfo("SessionService", std::string{"Sessions saved to file: path="} + path + 
        ", count=" + std::to_string(sessions_.size()));
    
    return true;
}

} // namespace req::shared
