# Entity Spawn/Update/Despawn Network Protocol - Implementation Summary

## Overview

Implemented a complete entity spawn/update/despawn protocol system that allows the ZoneServer to send NPCs and other players to clients when they enter the zone and during gameplay. This is a critical foundation for the Unreal Engine client to render the game world.

---

## Implementation Status

? **Protocol Design** - EntitySpawnData, EntityUpdateData, EntityDespawnData structs
? **Message Types** - EntitySpawn (44), EntityUpdate (45), EntityDespawn (46)
? **Serialization** - Build/parse functions for all entity messages
? **Server Implementation** - Entity tracking, spawn/update/despawn broadcasting
? **Interest Management** - Per-player tracking of known entities
? **Build Status** - Successful compilation with no errors

---

## Protocol Schemas

### EntitySpawn (Server ? Client)

**Purpose:** Notify client that an entity (player or NPC) has spawned in the zone.

**Wire Format:**
```
entityId|entityType|templateId|name|posX|posY|posZ|heading|level|hp|maxHp
```

**Fields:**
- `entityId` (uint64): Unique entity identifier (characterId for players, npcId for NPCs)
- `entityType` (uint32): 0=Player, 1=NPC
- `templateId` (uint32): Template/model ID (NPC template ID for NPCs, race ID for players)
- `name` (string): Display name (may contain spaces)
- `posX`, `posY`, `posZ` (float): Spawn position
- `heading` (float): Facing direction (0-360 degrees)
- `level` (uint32): Entity level
- `hp` (int32): Current HP
- `maxHp` (int32): Maximum HP

**Example:**
```
1001|1|5001|A Decaying Skeleton|100.0|50.0|0.0|90.0|1|20|20
```
*NPC instance 1001, template 5001, "A Decaying Skeleton", at (100,50,0), facing 90°, level 1, 20/20 HP*

**When Sent:**
- When player enters zone (for all existing NPCs and other players)
- When new NPC spawns
- When another player enters zone

---

### EntityUpdate (Server ? Client)

**Purpose:** Send periodic position/HP updates for NPCs (players use PlayerStateSnapshot).

**Wire Format:**
```
entityId|posX|posY|posZ|heading|hp|state
```

**Fields:**
- `entityId` (uint64): Entity identifier
- `posX`, `posY`, `posZ` (float): Current position
- `heading` (float): Facing direction (0-360 degrees)
- `hp` (int32): Current HP
- `state` (uint8): Entity state (0=Idle, 1=Combat, 2=Dead, etc.)

**Example:**
```
1001|105.5|52.3|0.0|95.0|15|1
```
*NPC 1001 at (105.5,52.3,0), facing 95°, 15 HP, state=Combat*

**When Sent:**
- Periodically for NPCs (e.g., 5-10 Hz) during simulation tick
- Only for NPCs within client's interest radius
- Only for NPCs the client knows about

---

### EntityDespawn (Server ? Client)

**Purpose:** Notify client that an entity has left the zone, died, or gone out of range.

**Wire Format:**
```
entityId|reason
```

**Fields:**
- `entityId` (uint64): Entity identifier
- `reason` (uint32): Despawn reason code

**Despawn Reasons:**
- `0` = Disconnect (player logged out)
- `1` = Death (entity died)
- `2` = Despawn (NPC respawn cycle)
- `3` = OutOfRange (entity left interest radius)

**Example:**
```
1001|1
```
*NPC 1001 died*

**When Sent:**
- When NPC dies
- When NPC respawns (despawn corpse, then spawn living version)
- When player disconnects
- When entity leaves client's interest radius

---

## Interest Management System

### Per-Player Entity Tracking

Each `ZonePlayer` now has a `knownEntities` set:

```cpp
struct ZonePlayer {
    // ...existing fields...
    
    // Entity tracking - which entities this player knows about
    std::unordered_set<std::uint64_t> knownEntities;
};
```

**Purpose:**
- Track which entities the client has received EntitySpawn messages for
- Prevent sending duplicate spawns
- Enable selective despawn (only send to clients that know the entity)
- Support interest radius filtering

**Workflow:**
1. Player enters zone ? `sendAllKnownEntities()` called
2. For each entity in zone, add to `player.knownEntities` and send EntitySpawn
3. During tick, only send EntityUpdate for entities in `knownEntities`
4. When entity despawns, remove from `knownEntities` and send EntityDespawn

---

## Server Implementation

### New File: ZoneServer_EntityMessages.cpp

Implements all entity broadcasting logic:

#### sendEntitySpawn(connection, entityId)
- Looks up entity (player or NPC)
- Builds EntitySpawnData struct
- Sends EntitySpawn message to connection
- Logs spawn event

#### broadcastEntitySpawn(entityId)
- Sends EntitySpawn to all connected players
- Adds entity to each player's knownEntities
- Skips sending to the entity itself (for players)

#### sendAllKnownEntities(connection, characterId)
- Called when player enters zone
- Sends EntitySpawn for all existing players (except self)
- Sends EntitySpawn for all alive NPCs
- Populates player's knownEntities set

#### sendEntityUpdate(connection, entityId)
- Builds EntityUpdateData for NPC
- Sends EntityUpdate message
- Only for NPCs (players use PlayerStateSnapshot)

#### broadcastEntityUpdates()
- Called during simulation tick
- For each player, sends updates for NPCs in their knownEntities
- Skips player entities (use PlayerStateSnapshot instead)

#### sendEntityDespawn(connection, entityId, reason)
- Builds EntityDespawnData
- Sends EntityDespawn message
- Logs despawn event

#### broadcastEntityDespawn(entityId, reason)
- Sends EntityDespawn to all players who know the entity
- Removes entity from each player's knownEntities
- Cleans up tracking state

---

## Integration Points

### 1. Zone Entry (ZoneAuthRequest Handler)

**Current:**
```cpp
// After successful zone auth...
connection->send(MessageType::ZoneAuthResponse, respBytes);
```

**TODO: Add Entity Spawns**
```cpp
// After successful zone auth...
connection->send(MessageType::ZoneAuthResponse, respBytes);

// NEW: Send all existing entities to the new player
sendAllKnownEntities(connection, characterId);
```

**Effect:** When player enters zone, they receive EntitySpawn for all NPCs and other players.

---

### 2. Simulation Tick (updateSimulation)

**Current:**
```cpp
// Update NPCs
for (auto& [npcId, npc] : npcs_) {
    updateNpc(npc, dt);
}
```

**TODO: Add Entity Updates**
```cpp
// Update NPCs
for (auto& [npcId, npc] : npcs_) {
    updateNpc(npc, dt);
}

// NEW: Broadcast NPC updates to clients
broadcastEntityUpdates();
```

**Effect:** Clients receive periodic position/HP updates for NPCs.

---

### 3. NPC Death (handlePlayerDeath or NPC death handler)

**Current:**
```cpp
npc.isAlive = false;
npc.currentHp = 0;
```

**TODO: Add Entity Despawn**
```cpp
npc.isAlive = false;
npc.currentHp = 0;

// NEW: Notify clients that NPC died
broadcastEntityDespawn(npc.npcId, 1);  // Reason 1 = Death
```

**Effect:** Clients remove dead NPC from their displays.

---

### 4. NPC Respawn (respawn logic)

**Current:**
```cpp
npc.currentHp = npc.maxHp;
npc.isAlive = true;
npc.posX = npc.spawnX;
npc.posY = npc.spawnY;
npc.posZ = npc.spawnZ;
```

**TODO: Add Entity Spawn**
```cpp
npc.currentHp = npc.maxHp;
npc.isAlive = true;
npc.posX = npc.spawnX;
npc.posY = npc.spawnY;
npc.posZ = npc.spawnZ;

// NEW: Notify clients that NPC respawned
broadcastEntitySpawn(npc.npcId);
```

**Effect:** Clients render NPC at respawn location.

---

### 5. Player Disconnect (removePlayer)

**Current:**
```cpp
// Save player state
savePlayerPosition(characterId);

// Remove from players map
players_.erase(it);
```

**TODO: Add Entity Despawn**
```cpp
// Save player state
savePlayerPosition(characterId);

// NEW: Notify other clients that player left
broadcastEntityDespawn(characterId, 0);  // Reason 0 = Disconnect

// Remove from players map
players_.erase(it);
```

**Effect:** Other clients remove disconnected player from their displays.

---

### 6. New Player Joins (after ZoneAuthRequest success)

**Current:**
```cpp
players_[characterId] = player;
connectionToCharacterId_[connection] = characterId;
```

**TODO: Add Entity Spawn Broadcast**
```cpp
players_[characterId] = player;
connectionToCharacterId_[connection] = characterId;

// NEW: Notify other clients about new player
broadcastEntitySpawn(characterId);
```

**Effect:** Existing clients see new player spawn.

---

## Unreal Engine Client Integration Guide

### Message Flow

```
????????????                          ????????????
?  Client  ?                          ?  Server  ?
????????????                          ????????????
     ?                                     ?
     ?  ZoneAuthRequest                   ?
     ?????????????????????????????????????>?
     ?                                     ?
     ?           ZoneAuthResponse (OK)     ?
     ?<?????????????????????????????????????
     ?                                     ?
     ?  EntitySpawn (NPC 1001)             ?
     ?<?????????????????????????????????????
     ?  EntitySpawn (NPC 1002)             ?
     ?<?????????????????????????????????????
     ?  EntitySpawn (Player 42)            ?
     ?<?????????????????????????????????????
     ?  ...                                ?
     ?                                     ?
     ?  (Periodic)                         ?
     ?  EntityUpdate (NPC 1001)            ?
     ?<?????????????????????????????????????
     ?  EntityUpdate (NPC 1002)            ?
     ?<?????????????????????????????????????
     ?  ...                                ?
     ?                                     ?
     ?  (NPC dies)                         ?
     ?  EntityDespawn (NPC 1001, reason=1) ?
     ?<?????????????????????????????????????
     ?                                     ?
     ?  (NPC respawns)                     ?
     ?  EntitySpawn (NPC 1001)             ?
     ?<?????????????????????????????????????
     ?                                     ?
```

---

### Client-Side Entity Manager (Recommended Architecture)

```cpp
// UE5 Example: UEntityManager.h

class UEntityManager : public UObject {
public:
    // Message Handlers
    void OnEntitySpawn(const FEntitySpawnData& Data);
    void OnEntityUpdate(const FEntityUpdateData& Data);
    void OnEntityDespawn(uint64 EntityId, uint32 Reason);
    
    // Entity Lookup
    AEntity* GetEntity(uint64 EntityId);
    
private:
    // Entity Registry
    TMap<uint64, AEntity*> Entities;
    
    // Entity Spawning
    AEntity* SpawnEntity(uint32 EntityType, uint32 TemplateId, const FVector& Position);
    void DespawnEntity(uint64 EntityId);
    
    // Update Interpolation
    void InterpolateEntity(AEntity* Entity, const FEntityUpdateData& Data);
};
```

---

### Client Message Routing

```cpp
// UE5 Example: OnMessageReceived()

void UNetworkClient::OnMessageReceived(uint16 MessageType, const FString& Payload) {
    switch (MessageType) {
        case 44: { // EntitySpawn
            FEntitySpawnData Data;
            if (ParseEntitySpawnPayload(Payload, Data)) {
                EntityManager->OnEntitySpawn(Data);
            }
            break;
        }
        
        case 45: { // EntityUpdate
            FEntityUpdateData Data;
            if (ParseEntityUpdatePayload(Payload, Data)) {
                EntityManager->OnEntityUpdate(Data);
            }
            break;
        }
        
        case 46: { // EntityDespawn
            uint64 EntityId;
            uint32 Reason;
            if (ParseEntityDespawnPayload(Payload, EntityId, Reason)) {
                EntityManager->OnEntityDespawn(EntityId, Reason);
            }
            break;
        }
    }
}
```

---

### Client Entity Spawning

```cpp
void UEntityManager::OnEntitySpawn(const FEntitySpawnData& Data) {
    // Check if entity already exists
    if (Entities.Contains(Data.EntityId)) {
        UE_LOG(LogTemp, Warning, TEXT("Entity %llu already spawned"), Data.EntityId);
        return;
    }
    
    // Determine actor class based on entityType
    TSubclassOf<AEntity> ActorClass;
    if (Data.EntityType == 0) {
        // Player
        ActorClass = PlayerEntityClass;
    } else if (Data.EntityType == 1) {
        // NPC - look up by templateId
        ActorClass = GetNpcActorClass(Data.TemplateId);
    }
    
    // Spawn actor
    FVector Position(Data.PosX, Data.PosY, Data.PosZ);
    FRotator Rotation(0, Data.Heading, 0);
    AEntity* Entity = GetWorld()->SpawnActor<AEntity>(ActorClass, Position, Rotation);
    
    // Initialize entity
    Entity->EntityId = Data.EntityId;
    Entity->SetDisplayName(Data.Name);
    Entity->SetLevel(Data.Level);
    Entity->SetHP(Data.HP, Data.MaxHP);
    
    // Register entity
    Entities.Add(Data.EntityId, Entity);
    
    UE_LOG(LogTemp, Log, TEXT("Spawned entity %llu: %s"), Data.EntityId, *Data.Name);
}
```

---

### Client Entity Updates

```cpp
void UEntityManager::OnEntityUpdate(const FEntityUpdateData& Data) {
    AEntity* Entity = GetEntity(Data.EntityId);
    if (!Entity) {
        UE_LOG(LogTemp, Warning, TEXT("Update for unknown entity %llu"), Data.EntityId);
        return;
    }
    
    // Update target position for interpolation
    Entity->SetTargetPosition(FVector(Data.PosX, Data.PosY, Data.PosZ));
    Entity->SetTargetRotation(FRotator(0, Data.Heading, 0));
    Entity->SetHP(Data.HP);
    Entity->SetState(Data.State);
    
    // Interpolation happens in Entity's Tick() function
}
```

---

### Client Entity Despawning

```cpp
void UEntityManager::OnEntityDespawn(uint64 EntityId, uint32 Reason) {
    AEntity* Entity = GetEntity(EntityId);
    if (!Entity) {
        UE_LOG(LogTemp, Warning, TEXT("Despawn for unknown entity %llu"), EntityId);
        return;
    }
    
    // Play death/despawn effects based on reason
    if (Reason == 1) {
        // Death - play death animation
        Entity->PlayDeathAnimation();
    } else if (Reason == 0) {
        // Disconnect - fade out
        Entity->FadeOut();
    }
    
    // Schedule destruction after animation
    FTimerHandle Handle;
    GetWorld()->GetTimerManager().SetTimer(Handle, [this, EntityId]() {
        DespawnEntity(EntityId);
    }, 2.0f, false);
}

void UEntityManager::DespawnEntity(uint64 EntityId) {
    AEntity* Entity = Entities.FindAndRemoveChecked(EntityId);
    Entity->Destroy();
    
    UE_LOG(LogTemp, Log, TEXT("Despawned entity %llu"), EntityId);
}
```

---

### Client Position Interpolation

```cpp
// In AEntity::Tick(float DeltaTime)

void AEntity::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
    
    // Interpolate position
    FVector CurrentPos = GetActorLocation();
    FVector NewPos = FMath::VInterpTo(CurrentPos, TargetPosition, DeltaTime, InterpolationSpeed);
    SetActorLocation(NewPos);
    
    // Interpolate rotation
    FRotator CurrentRot = GetActorRotation();
    FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRotation, DeltaTime, InterpolationSpeed);
    SetActorRotation(NewRot);
}
```

**Key Points:**
- Server sends discrete positions at 5-10 Hz
- Client interpolates between them for smooth movement
- Use `VInterpTo` or `VInterpConstantTo` for position
- Use `RInterpTo` for rotation
- Tune `InterpolationSpeed` based on server tick rate

---

## Testing with TestClient

### Add Entity Message Handlers

**File:** `REQ_TestClient/src/TestClient_Movement.cpp`

```cpp
// In tryReceiveMessage() loop

if (header.type == req::shared::MessageType::EntitySpawn) {
    req::shared::protocol::EntitySpawnData spawn;
    if (req::shared::protocol::parseEntitySpawnPayload(msgBody, spawn)) {
        std::cout << "[ENTITY_SPAWN] "
                 << "id=" << spawn.entityId << " "
                 << "type=" << (spawn.entityType == 0 ? "Player" : "NPC") << " "
                 << "name=\"" << spawn.name << "\" "
                 << "pos=(" << spawn.posX << "," << spawn.posY << "," << spawn.posZ << ") "
                 << "level=" << spawn.level << " "
                 << "hp=" << spawn.hp << "/" << spawn.maxHp << std::endl;
    }
}

if (header.type == req::shared::MessageType::EntityUpdate) {
    req::shared::protocol::EntityUpdateData update;
    if (req::shared::protocol::parseEntityUpdatePayload(msgBody, update)) {
        std::cout << "[ENTITY_UPDATE] "
                 << "id=" << update.entityId << " "
                 << "pos=(" << update.posX << "," << update.posY << "," << update.posZ << ") "
                 << "heading=" << update.heading << " "
                 << "hp=" << update.hp << " "
                 << "state=" << (int)update.state << std::endl;
    }
}

if (header.type == req::shared::MessageType::EntityDespawn) {
    req::shared::protocol::EntityDespawnData despawn;
    if (req::shared::protocol::parseEntityDespawnPayload(msgBody, despawn)) {
        std::cout << "[ENTITY_DESPAWN] "
                 << "id=" << despawn.entityId << " "
                 << "reason=" << despawn.reason << " "
                 << "(0=Disconnect, 1=Death, 2=Despawn, 3=OutOfRange)" << std::endl;
    }
}
```

---

### Expected TestClient Output

```
[ENTITY_SPAWN] id=1001 type=NPC name="A Decaying Skeleton" pos=(100,50,0) level=1 hp=20/20
[ENTITY_SPAWN] id=1002 type=NPC name="A Rat" pos=(200,75,0) level=1 hp=10/10
[ENTITY_SPAWN] id=1003 type=NPC name="A Fire Beetle" pos=(150,120,0) level=2 hp=30/30
[ENTITY_SPAWN] id=1004 type=NPC name="A Snake" pos=(50,200,0) level=1 hp=15/15
[ENTITY_SPAWN] id=1005 type=NPC name="A Zombie" pos=(300,150,0) level=3 hp=50/50

[ENTITY_UPDATE] id=1001 pos=(105.5,52.3,0) heading=95.0 hp=20 state=0
[ENTITY_UPDATE] id=1002 pos=(202.1,76.8,0) heading=180.0 hp=10 state=0
...

[ENTITY_DESPAWN] id=1001 reason=1 (0=Disconnect, 1=Death, 2=Despawn, 3=OutOfRange)
```

---

## Performance Considerations

### 1. EntitySpawn on Zone Entry

**Scenario:** Player enters zone with 50 NPCs + 10 other players

**Messages Sent:** 60 EntitySpawn messages
**Payload Size:** ~150 bytes each = ~9 KB total
**Impact:** One-time cost, acceptable

**Optimization:** Could batch into single message with multiple entities (future)

---

### 2. EntityUpdate Tick Rate

**Current:** Not yet integrated (TODO)

**Recommended:** 5-10 Hz (200ms - 100ms intervals)

**For 50 NPCs at 10 Hz:**
- 500 messages/sec to each player
- ~50 bytes each = 25 KB/sec per player
- For 10 players = 250 KB/sec total

**Optimization Strategies:**
- Send updates only for NPCs within interest radius (e.g., 200 units)
- Reduce tick rate for distant NPCs (e.g., 2 Hz for far NPCs)
- Delta compression (send only changed fields)
- Spatial partitioning (only update nearby cells)

---

### 3. Interest Radius Filtering

**Current:** All NPCs sent on zone entry

**Future:**
```cpp
void sendAllKnownEntities(ConnectionPtr connection, std::uint64_t characterId) {
    auto playerIt = players_.find(characterId);
    // ...
    
    // Only send NPCs within interest radius
    const float interestRadius = zoneConfig_.interestRadius;  // e.g., 2000 units
    
    for (const auto& [npcId, npc] : npcs_) {
        if (!npc.isAlive) continue;
        
        // Calculate distance
        float dx = npc.posX - player.posX;
        float dy = npc.posY - player.posY;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist <= interestRadius) {
            player.knownEntities.insert(npcId);
            sendEntitySpawn(connection, npcId);
        }
    }
}
```

**Effect:** Reduces initial spawn count, improves scalability

---

## Future Enhancements

### 1. Batch Entity Messages

**Idea:** Send multiple entities in single message

**Example:**
```
EntitySpawnBatch|count|entity1Data|entity2Data|...
```

**Benefit:** Reduces message overhead, improves network efficiency

---

### 2. Delta Compression for Updates

**Idea:** Only send changed fields

**Example:**
```
EntityUpdateDelta|entityId|changedFieldsMask|...values...
```

**Mask:**
- Bit 0: Position changed
- Bit 1: Heading changed
- Bit 2: HP changed
- Bit 3: State changed

**Benefit:** Reduces update packet size by 50-70%

---

### 3. Spatial Interest Management

**Idea:** Divide zone into grid cells, only update visible cells

**Implementation:**
```cpp
struct ZoneCell {
    std::unordered_set<std::uint64_t> entities;
};

std::unordered_map<CellId, ZoneCell> cells_;
```

**Benefit:** Scales to large zones with hundreds of entities

---

### 4. Priority-Based Updates

**Idea:** Update nearby entities more frequently than distant ones

**Example:**
- Distance 0-50: 20 Hz
- Distance 50-100: 10 Hz
- Distance 100-200: 5 Hz
- Distance >200: 2 Hz

**Benefit:** Smooth nearby movement, reduced network load

---

## Summary

? **Protocol Schema** - Clean, documented message formats
? **Server Infrastructure** - Entity tracking, spawning, updates, despawns
? **Interest Management** - Per-player known entity tracking
? **Extensible Design** - Easy to add new entity types, optimize later
? **Build Status** - Compiles successfully, ready for integration
? **Documentation** - Complete guide for UE5 client implementation

**Next Steps:**
1. Integrate entity spawns into ZoneAuthRequest handler
2. Integrate entity updates into simulation tick
3. Integrate entity despawns into NPC death/respawn/player disconnect
4. Test with TestClient to verify message flow
5. Implement UE5 client entity manager based on this guide

**Status:** Core protocol complete, integration TODO (step 5-8)

---

## Files Created/Modified

### Created:
- `REQ_ZoneServer/src/ZoneServer_EntityMessages.cpp` - Entity broadcasting logic

### Modified:
- `REQ_Shared/include/req/shared/Protocol_Zone.h` - EntitySpawnData, EntityUpdateData, EntityDespawnData structs + function declarations
- `REQ_Shared/src/Protocol_Zone.cpp` - Build/parse implementations for entity messages
- `REQ_Shared/include/req/shared/MessageTypes.h` - EntitySpawn (44), EntityUpdate (45), EntityDespawn (46) message types
- `REQ_ZoneServer/include/req/zone/ZoneServer.h` - Entity broadcasting method declarations + knownEntities tracking in ZonePlayer

**Total:** 5 files modified/created, 0 build errors

