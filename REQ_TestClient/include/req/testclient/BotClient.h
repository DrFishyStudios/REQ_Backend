#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <chrono>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::testclient {

/**
 * BotConfig
 * 
 * Configuration for a single bot instance.
 */
struct BotConfig {
    std::string username;
    std::string password;
    std::int32_t targetWorldId{ 1 };
    std::int32_t startingZoneId{ 10 };
    
    // Movement pattern config
    enum class MovementPattern {
        Circle,         // Move in a circle
        BackAndForth,   // Move back and forth on X axis
        Random,         // Random walk
        Stationary      // Don't move (just stand still)
    };
    
    MovementPattern pattern{ MovementPattern::Circle };
    float moveRadius{ 50.0f };        // Radius for circle pattern
    float angularSpeed{ 0.5f };       // Radians per second for circle
    float walkSpeed{ 5.0f };          // Units per second for back-and-forth
    
    // Logging verbosity
    enum class LogLevel {
        Minimal,    // Only major events (login, zone entry)
        Normal,     // Include movement send/snapshot receive
        Debug       // Everything including snapshot details
    };
    
    LogLevel logLevel{ LogLevel::Minimal };
};

/**
 * BotClient
 * 
 * Encapsulates a single bot instance that can autonomously:
 * - Connect to Login/World/Zone servers
 * - Authenticate and enter a zone
 * - Run scripted movement patterns
 * - Log snapshot data for testing interest management
 */
class BotClient {
public:
    BotClient(int botIndex);
    ~BotClient();
    
    // Main bot lifecycle
    void start(const BotConfig& cfg);
    void tick();  // Called periodically from main loop
    void stop();
    
    // State queries
    bool isRunning() const { return running_; }
    bool isInZone() const { return inZone_; }
    int getBotIndex() const { return botIndex_; }
    std::uint64_t getCharacterId() const { return characterId_; }
    
    // Snapshot handling
    void handleSnapshot(const req::shared::protocol::PlayerStateSnapshotData& snapshot);

private:
    using Tcp = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;
    
    // Bot identity
    int botIndex_;
    BotConfig config_;
    
    // State flags
    bool running_{ false };
    bool inZone_{ false };
    bool authenticated_{ false };
    
    // Connection state
    std::shared_ptr<boost::asio::io_context> ioContext_;
    std::shared_ptr<Tcp::socket> zoneSocket_;
    
    // Session state
    req::shared::SessionToken sessionToken_{ 0 };
    req::shared::WorldId worldId_{ 0 };
    std::string worldHost_;
    std::uint16_t worldPort_{ 0 };
    
    req::shared::HandoffToken handoffToken_{ 0 };
    req::shared::ZoneId zoneId_{ 0 };
    std::string zoneHost_;
    std::uint16_t zonePort_{ 0 };
    
    std::uint64_t characterId_{ 0 };
    
    // Movement state
    std::uint32_t movementSequence_{ 0 };
    float movementAngle_{ 0.0f };      // Current angle for circle pattern
    float movementPhase_{ 0.0f };      // Phase for back-and-forth
    float centerX_{ 0.0f };            // Center point for movement
    float centerY_{ 0.0f };
    
    // Current position (from snapshots)
    float posX_{ 0.0f };
    float posY_{ 0.0f };
    float posZ_{ 0.0f };
    
    // Timing
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point lastTickTime_;
    std::chrono::steady_clock::time_point lastMovementTime_;
    
    // Handshake stages
    bool doLogin();
    bool doCharacterList();
    bool doCharacterCreate();
    bool doEnterWorld();
    bool doZoneAuth();
    
    // Movement logic
    void updateMovement(float deltaTime);
    void sendMovementIntent(float inputX, float inputY, float yaw, bool jump);
    
    // Network helpers
    bool sendMessage(Tcp::socket& socket, req::shared::MessageType type, const std::string& body);
    bool receiveMessage(Tcp::socket& socket, req::shared::MessageHeader& outHeader, std::string& outBody);
    bool tryReceiveMessage(Tcp::socket& socket, req::shared::MessageHeader& outHeader, std::string& outBody);
    
    // Logging helpers
    void logMinimal(const std::string& msg);
    void logNormal(const std::string& msg);
    void logDebug(const std::string& msg);
    std::string getBotPrefix() const;
};

} // namespace req::testclient
