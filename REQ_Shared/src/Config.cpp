#include "../include/req/shared/Config.h"

#include <string>

namespace req::shared {

LoginConfig loadLoginConfig(const std::string& path) {
    // TODO: Parse JSON at path (config/login_config.json)
    (void)path;
    LoginConfig cfg{}; // default values
    return cfg;
}

WorldConfig loadWorldConfig(const std::string& path) {
    // TODO: Parse JSON at path (config/world_config.json)
    (void)path;
    WorldConfig cfg{}; // default values
    return cfg;
}

} // namespace req::shared
