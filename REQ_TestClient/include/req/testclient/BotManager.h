#pragma once

#include <vector>
#include <memory>
#include <string>

#include "BotClient.h"

namespace req::testclient {

/**
 * BotManager
 * 
 * Manages multiple bot instances, handling their lifecycle and tick updates.
 */
class BotManager {
public:
    BotManager();
    ~BotManager();
    
    // Spawn N bots with default or custom config
    void spawnBots(int count, const BotConfig& baseConfig = BotConfig{});
    
    // Main loop - ticks all bots
    void run();
    
    // Stop all bots
    void stopAll();
    
    // Get bot statistics
    int getTotalBots() const { return static_cast<int>(bots_.size()); }
    int getActiveBots() const;
    int getBotsInZone() const;

private:
    std::vector<std::unique_ptr<BotClient>> bots_;
    bool running_{ false };
};

} // namespace req::testclient
