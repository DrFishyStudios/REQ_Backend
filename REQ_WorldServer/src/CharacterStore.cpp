#include "../include/req/world/CharacterStore.h"

#include <string>
#include <vector>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

namespace req::world {

std::vector<req::shared::data::PlayerCore> CharacterStore::loadCharacters(req::shared::AccountId accountId) {
    (void)accountId; // suppress unused for now
    // TODO: Load from data/saves/players/<accountId>.json
    req::shared::logInfo("world", "CharacterStore::loadCharacters called (stub)");
    return {};
}

bool CharacterStore::saveCharacter(const req::shared::data::PlayerCore& player) {
    (void)player; // suppress unused for now
    // TODO: Write to data/saves/players/<accountId>.json
    req::shared::logInfo("world", "CharacterStore::saveCharacter called (stub)");
    return true;
}

} // namespace req::world
