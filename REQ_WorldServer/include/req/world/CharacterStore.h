#pragma once

#include <vector>
#include <cstdint>

namespace req::shared {
    using AccountId = std::uint64_t; // mirror alias from req::shared::Types.hpp
    namespace data { struct PlayerCore; }
}

namespace req::world {

class CharacterStore {
public:
    CharacterStore() = default;

    std::vector<req::shared::data::PlayerCore> loadCharacters(req::shared::AccountId accountId);
    bool saveCharacter(const req::shared::data::PlayerCore& player);

    // TODO: Add methods to load/save single characters, delete, etc.
};

} // namespace req::world
