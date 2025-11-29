#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <random>

namespace req::shared {

/*
 * SessionRecord
 * 
 * Represents an authenticated session for an account.
 * 
 * Sessions are created by LoginServer after successful authentication and
 * validated by WorldServer/ZoneServer during the handshake process.
 * 
 * Fields:
 *   - sessionToken: Unique 64-bit random identifier for this session
 *   - accountId: Account that owns this session
 *   - createdAt: When the session was created
 *   - lastSeen: Last time this session was validated (updated on each use)
 *   - boundWorldId: World this session is currently bound to (-1 if unbound)
 * 
 * Note: Sessions are stored in-memory only (no cross-process sharing).
 *       For multi-server deployments, consider Redis or similar.
 */
struct SessionRecord {
    std::uint64_t sessionToken{ 0 };
    std::uint64_t accountId{ 0 };
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastSeen;
    std::int32_t boundWorldId{ -1 };  // -1 = not bound to any world
};

/*
 * SessionService
 * 
 * Shared session management service for REQ backend servers.
 * 
 * Responsibilities:
 *   - Create sessions after login (LoginServer)
 *   - Validate sessions during handshake (WorldServer, ZoneServer)
 *   - Track session-to-world binding
 *   - Provide session lifecycle management
 * 
 * Thread Safety:
 *   All public methods are thread-safe (protected by internal mutex).
 * 
 * Session Tokens:
 *   - 64-bit random values generated using std::mt19937_64
 *   - Not cryptographically secure (sufficient for emulator purposes)
 *   - Collision probability is negligible for reasonable session counts
 * 
 * Persistence:
 *   - Currently in-memory only
 *   - Optional JSON persistence for development/debugging
 *   - Production deployments should use Redis or similar
 * 
 * Usage Example:
 * 
 *   // LoginServer: Create session after successful login
 *   auto& sessionService = SessionService::instance();
 *   uint64_t token = sessionService.createSession(accountId);
 *   
 *   // WorldServer: Validate session
 *   auto session = sessionService.validateSession(token);
 *   if (session.has_value()) {
 *       uint64_t accountId = session->accountId;
 *       // ... proceed with handshake ...
 *   }
 *   
 *   // Bind to world
 *   sessionService.bindSessionToWorld(token, worldId);
 *   
 *   // Logout: Remove session
 *   sessionService.removeSession(token);
 */
class SessionService {
public:
    // Singleton access
    static SessionService& instance();
    
    // Disable copy/move
    SessionService(const SessionService&) = delete;
    SessionService& operator=(const SessionService&) = delete;
    SessionService(SessionService&&) = delete;
    SessionService& operator=(SessionService&&) = delete;
    
    /*
     * Create a new session for an account
     * 
     * Generates a unique 64-bit session token and stores a SessionRecord.
     * The session is initially unbound to any world (boundWorldId = -1).
     * 
     * Thread-safe.
     * 
     * @param accountId - Account ID that owns this session
     * @return Unique session token
     */
    std::uint64_t createSession(std::uint64_t accountId);
    
    /*
     * Validate a session token
     * 
     * Looks up the session by token. If found, updates lastSeen timestamp
     * and returns the SessionRecord. If not found, returns std::nullopt.
     * 
     * Thread-safe.
     * 
     * @param sessionToken - Token to validate
     * @return SessionRecord if valid, std::nullopt if not found
     */
    std::optional<SessionRecord> validateSession(std::uint64_t sessionToken);
    
    /*
     * Bind a session to a world
     * 
     * Associates the session with a specific worldId. This is typically
     * called when a client connects to a WorldServer.
     * 
     * If the session doesn't exist, logs a warning and does nothing.
     * 
     * Thread-safe.
     * 
     * @param sessionToken - Session to bind
     * @param worldId - World ID to bind to
     */
    void bindSessionToWorld(std::uint64_t sessionToken, std::int32_t worldId);
    
    /*
     * Remove a session
     * 
     * Deletes the session from the in-memory store. Typically called
     * on logout or session timeout.
     * 
     * Thread-safe.
     * 
     * @param sessionToken - Session to remove
     */
    void removeSession(std::uint64_t sessionToken);
    
    /*
     * Get session count (for monitoring/debugging)
     * 
     * Thread-safe.
     * 
     * @return Number of active sessions
     */
    std::size_t getSessionCount() const;
    
    /*
     * Clear all sessions (for testing/shutdown)
     * 
     * Thread-safe.
     */
    void clearAllSessions();
    
    // Optional: JSON persistence for development/debugging
    // Note: These are basic implementations and not optimized for production
    
    /*
     * Load sessions from JSON file
     * 
     * Replaces current in-memory sessions with loaded data.
     * Typically called at server startup.
     * 
     * Thread-safe.
     * 
     * @param path - Path to JSON file
     * @return true if loaded successfully, false on error
     */
    bool loadFromFile(const std::string& path);
    
    /*
     * Save sessions to JSON file
     * 
     * Writes current in-memory sessions to a JSON file.
     * Typically called periodically or on shutdown.
     * 
     * Thread-safe.
     * 
     * @param path - Path to JSON file
     * @return true if saved successfully, false on error
     */
    bool saveToFile(const std::string& path) const;

private:
    SessionService() = default;
    ~SessionService() = default;
    
    // Generate a unique session token
    std::uint64_t generateSessionToken();
    
    // Session storage
    std::unordered_map<std::uint64_t, SessionRecord> sessions_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Random number generator for session tokens
    // Note: Not cryptographically secure, but sufficient for emulator
    std::mt19937_64 rng_;
};

} // namespace req::shared
