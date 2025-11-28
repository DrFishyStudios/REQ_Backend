#pragma once

#include <string>
#include "Types.h"

namespace req::shared {

struct LoginConfig {
    std::string address{ "0.0.0.0" };
    std::uint16_t port{ 7777 };
    std::string worldHost{ "127.0.0.1" };
    std::uint16_t worldPort{ 7778 };
};

struct WorldConfig {
    std::string address{ "0.0.0.0" };
    std::uint16_t port{ 7778 };
    std::string zoneHost{ "127.0.0.1" };
    std::uint16_t zonePort{ 7779 };
};

LoginConfig loadLoginConfig(const std::string& path);
WorldConfig loadWorldConfig(const std::string& path);

} // namespace req::shared
