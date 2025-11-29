#include "../include/req/testclient/BotManager.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>

#include "../../REQ_Shared/include/req/shared/Logger.h"

namespace req::testclient {

namespace {
    // Global pointer for signal handler
    BotManager* g_botManager = nullptr;
    
    void signalHandler(int signal) {
        std::cout << "\nReceived signal " << signal << ", shutting down bots gracefully...\n";
        if (g_botManager) {
            g_botManager->stopAll();
        }
    }
}

BotManager::BotManager() {
    g_botManager = this;
    
    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
}

BotManager::~BotManager() {
    stopAll();
    g_botManager = nullptr;
}

void BotManager::spawnBots(int count, const BotConfig& baseConfig) {
    req::shared::logInfo("BotManager", std::string{"Spawning "} + std::to_string(count) + " bot(s)...");
    
    for (int i = 0; i < count; ++i) {
        // Create config for this bot
        BotConfig botConfig = baseConfig;
        
        // Generate unique username
        std::ostringstream oss;
        oss << "Bot" << std::setfill('0') << std::setw(3) << (i + 1);
        botConfig.username = oss.str();
        botConfig.password = "botpass";  // Simple password for all bots
        
        // Vary movement patterns for visual interest
        switch (i % 4) {
        case 0: botConfig.pattern = BotConfig::MovementPattern::Circle; break;
        case 1: botConfig.pattern = BotConfig::MovementPattern::BackAndForth; break;
        case 2: botConfig.pattern = BotConfig::MovementPattern::Random; break;
        case 3: botConfig.pattern = BotConfig::MovementPattern::Stationary; break;
        }
        
        // Vary movement parameters slightly
        botConfig.moveRadius = 50.0f + (i * 10.0f);
        botConfig.angularSpeed = 0.5f + (i * 0.1f);
        
        // Create and start bot
        auto bot = std::make_unique<BotClient>(i + 1);
        
        req::shared::logInfo("BotManager", std::string{"Starting bot "} + std::to_string(i + 1) + 
            "/" + std::to_string(count) + " (" + botConfig.username + ")...");
        
        bot->start(botConfig);
        
        // Small delay between bot spawns to avoid overwhelming servers
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (bot->isRunning()) {
            bots_.push_back(std::move(bot));
            req::shared::logInfo("BotManager", std::string{"Bot "} + botConfig.username + " started successfully");
        } else {
            req::shared::logWarn("BotManager", std::string{"Bot "} + botConfig.username + " failed to start");
        }
    }
    
    req::shared::logInfo("BotManager", std::string{"Bot spawning complete: "} + 
        std::to_string(getActiveBots()) + "/" + std::to_string(count) + " bots active");
}

void BotManager::run() {
    running_ = true;
    
    req::shared::logInfo("BotManager", "Bot manager main loop starting");
    req::shared::logInfo("BotManager", "Press Ctrl+C to stop all bots and exit");
    
    std::cout << "\n=== Bot Status ===\n";
    std::cout << "Total bots: " << getTotalBots() << "\n";
    std::cout << "Active bots: " << getActiveBots() << "\n";
    std::cout << "Bots in zone: " << getBotsInZone() << "\n";
    std::cout << "==================\n\n";
    
    auto lastStatusUpdate = std::chrono::steady_clock::now();
    
    while (running_ && getActiveBots() > 0) {
        // Tick all bots
        for (auto& bot : bots_) {
            if (bot && bot->isRunning()) {
                bot->tick();
            }
        }
        
        // Print status update every 10 seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusUpdate).count();
        if (elapsed >= 10) {
            std::cout << "\n=== Bot Status Update ===\n";
            std::cout << "Active bots: " << getActiveBots() << "/" << getTotalBots() << "\n";
            std::cout << "Bots in zone: " << getBotsInZone() << "\n";
            std::cout << "=========================\n\n";
            lastStatusUpdate = now;
        }
        
        // Sleep for a short interval (bot tick rate)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    req::shared::logInfo("BotManager", "Bot manager main loop exiting");
}

void BotManager::stopAll() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    req::shared::logInfo("BotManager", "Stopping all bots...");
    
    for (auto& bot : bots_) {
        if (bot && bot->isRunning()) {
            bot->stop();
        }
    }
    
    req::shared::logInfo("BotManager", "All bots stopped");
}

int BotManager::getActiveBots() const {
    int count = 0;
    for (const auto& bot : bots_) {
        if (bot && bot->isRunning()) {
            count++;
        }
    }
    return count;
}

int BotManager::getBotsInZone() const {
    int count = 0;
    for (const auto& bot : bots_) {
        if (bot && bot->isInZone()) {
            count++;
        }
    }
    return count;
}

} // namespace req::testclient
