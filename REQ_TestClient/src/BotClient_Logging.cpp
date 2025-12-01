#include "../include/req/testclient/BotClient.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

namespace req::testclient {

void BotClient::logMinimal(const std::string& msg) {
    if (config_.logLevel >= BotConfig::LogLevel::Minimal) {
        std::cout << getBotPrefix() << msg << std::endl;
    }
}

void BotClient::logNormal(const std::string& msg) {
    if (config_.logLevel >= BotConfig::LogLevel::Normal) {
        std::cout << getBotPrefix() << msg << std::endl;
    }
}

void BotClient::logDebug(const std::string& msg) {
    if (config_.logLevel >= BotConfig::LogLevel::Debug) {
        std::cout << getBotPrefix() << msg << std::endl;
    }
}

std::string BotClient::getBotPrefix() const {
    std::ostringstream oss;
    oss << "[Bot" << std::setfill('0') << std::setw(3) << botIndex_ << "] ";
    return oss.str();
}

} // namespace req::testclient
