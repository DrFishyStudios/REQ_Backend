#include "../include/req/testclient/BotClient.h"

#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"

#include <cmath>
#include <random>
#include <chrono>

namespace req::testclient {

namespace {
    std::uint32_t getClientTimeMs() {
        static auto g_startTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime);
        return static_cast<std::uint32_t>(duration.count());
    }
}

void BotClient::updateMovement(float deltaTime) {
    float inputX = 0.0f;
    float inputY = 0.0f;
    float yaw = 0.0f;
    
    switch (config_.pattern) {
    case BotConfig::MovementPattern::Circle: {
        // Move in a circle around center point
        movementAngle_ += config_.angularSpeed * deltaTime;
        
        // Normalize angle to [0, 2*pi]
        while (movementAngle_ >= 2.0f * 3.14159f) {
            movementAngle_ -= 2.0f * 3.14159f;
        }
        
        // Movement direction (tangent to circle)
        float dirX = -std::sin(movementAngle_);
        float dirY = std::cos(movementAngle_);
        
        inputX = dirX;
        inputY = dirY;
        yaw = std::atan2(dirX, dirY) * 180.0f / 3.14159f;
        break;
    }
    
    case BotConfig::MovementPattern::BackAndForth: {
        // Move back and forth on X axis
        movementPhase_ += config_.walkSpeed * deltaTime;
        
        // Oscillate between -radius and +radius
        if (movementPhase_ > config_.moveRadius) {
            movementPhase_ = config_.moveRadius;
            config_.walkSpeed = -std::abs(config_.walkSpeed);
        } else if (movementPhase_ < -config_.moveRadius) {
            movementPhase_ = -config_.moveRadius;
            config_.walkSpeed = std::abs(config_.walkSpeed);
        }
        
        inputX = (config_.walkSpeed > 0.0f) ? 1.0f : -1.0f;
        inputY = 0.0f;
        yaw = (config_.walkSpeed > 0.0f) ? 90.0f : 270.0f;
        break;
    }
    
    case BotConfig::MovementPattern::Random: {
        // Random walk (change direction periodically)
        static std::mt19937 rng(botIndex_);  // Seed with bot index
        static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        static std::uniform_real_distribution<float> yawDist(0.0f, 360.0f);
        
        // Change direction every 2 seconds
        static float timeSinceDirectionChange = 0.0f;
        timeSinceDirectionChange += deltaTime;
        
        if (timeSinceDirectionChange >= 2.0f) {
            inputX = dist(rng);
            inputY = dist(rng);
            yaw = yawDist(rng);
            timeSinceDirectionChange = 0.0f;
        }
        break;
    }
    
    case BotConfig::MovementPattern::Stationary:
    default:
        // Don't move
        inputX = 0.0f;
        inputY = 0.0f;
        yaw = 0.0f;
        break;
    }
    
    sendMovementIntent(inputX, inputY, yaw, false);
}

void BotClient::sendMovementIntent(float inputX, float inputY, float yaw, bool jump) {
    if (!zoneSocket_ || !zoneSocket_->is_open()) {
        return;
    }
    
    req::shared::protocol::MovementIntentData intent;
    intent.characterId = characterId_;
    intent.sequenceNumber = ++movementSequence_;
    intent.inputX = inputX;
    intent.inputY = inputY;
    intent.facingYawDegrees = yaw;
    intent.isJumpPressed = jump;
    intent.clientTimeMs = getClientTimeMs();
    
    std::string payload = req::shared::protocol::buildMovementIntentPayload(intent);
    if (sendMessage(*zoneSocket_, req::shared::MessageType::MovementIntent, payload)) {
        logDebug("Sent movement: seq=" + std::to_string(intent.sequenceNumber) + 
                ", input=(" + std::to_string(inputX) + "," + std::to_string(inputY) + ")");
    }
}

} // namespace req::testclient
