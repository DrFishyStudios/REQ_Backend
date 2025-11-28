#pragma once

#include <string>
#include <cstdint>

namespace req::shared {
    using ZoneId = std::uint32_t; // mirror alias from req::shared::Types.hpp
}

namespace req::zone {

class ZoneInstance {
public:
    ZoneInstance(req::shared::ZoneId id, std::string name);

    void loadStaticData(); // TODO: use DataLoader later
    void update(float deltaSeconds);

private:
    req::shared::ZoneId id_{};
    std::string name_;
};

} // namespace req::zone
