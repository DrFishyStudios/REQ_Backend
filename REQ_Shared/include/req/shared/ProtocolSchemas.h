#pragma once

// Domain-specific protocol headers
#include "Protocol_Login.h"
#include "Protocol_World.h"
#include "Protocol_Zone.h"
#include "Protocol_Character.h"
#include "Protocol_Combat.h"

/*
 * ProtocolSchemas.h
 * 
 * Wire-level payload format definitions for REQ backend handshake protocol.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 * This matches section 14.5 of the GDD.
 * 
 * Protocol Version: 1
 * 
 * This header now serves as a façade that includes all domain-specific protocol headers:
 *   - Protocol_Login.h (LoginRequest, LoginResponse, etc.)
 *   - Protocol_World.h (WorldAuthRequest, WorldAuthResponse, etc.)
 *   - Protocol_Zone.h (ZoneAuthRequest, ZoneAuthResponse, Movement, PlayerStateSnapshot, etc.)
 *   - Protocol_Character.h (CharacterList, CharacterCreate, EnterWorld, etc.)
 *   - Protocol_Combat.h (AttackRequest, AttackResult, etc.)
 * 
 * All declarations remain in the req::shared::protocol namespace.
 * 
 * For new code, consider including only the specific protocol header you need
 * instead of this façade header to reduce compilation dependencies.
 */
