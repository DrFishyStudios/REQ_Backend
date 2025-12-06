REQ – Game Design Document (v0.9)

# Project Overview

**Project Name:** REQ

**Genre:** Classic fantasy MMORPG (tab-target, group-centric, multi-zone)

**Platform:** PC (Windows); future-proofed for Linux servers

**Engine:** Unreal Engine 5.7 client + C++ authoritative server stack

**Target:** Learning project, vertical slice first, expandable architecture

**Emulator Replacement Goal:**  
REQ is intended as a modern spiritual successor and technical replacement for classic EverQuest-style emulator projects (e.g., Project 1999), reproducing the core gameplay feel while using entirely original code and art assets. The aim is to provide a stable, extensible platform where different servers can choose how closely they mirror classic rulesets versus enabling modern quality-of-life features.

---

## 1. High Concept

REQ aims to capture the essence of classic late-90s/early-00s MMORPGs while leveraging modern technology. This section defines the core vision.

A modern build of a late-90s / early-00s style MMO: a dangerous open world with slower combat, meaningful grouping, exploration, crafting, and social dependency. Players begin in safe settlements and venture into increasingly hostile wilderness and ruins.

---

## 1.1 Relationship to Classic EverQuest & Emulator Scene

REQ is explicitly inspired by classic EverQuest-era design but is not a client or asset re-use project. It uses:

- A custom, Unreal Engine–based client.
- A bespoke C++ server stack (login, world, zone, chat).
- Original 3D models, animations, effects, UI, and audio.

The project's goals relative to existing emulators (such as Project 1999) are:

**Mechanical homage:** Recreate the pacing, danger, class interdependence, and faction dynamics of Luclin-era EQ-style gameplay.

**Modernized stack:** Replace legacy emulator code with a cleaner, more modular architecture suitable for learning, modding, and long-term maintenance.

**Configurable "era feel":** Allow individual servers to toggle rule variants (e.g., XP rate, death penalties, raid instancing, UI assists) to approximate different eras or custom experiences.

**IP-safe content:** Ensure all art, audio, naming, and written content are original or generic fantasy, avoiding direct reuse of proprietary assets or lore.

REQ should feel instantly familiar to players who love P99-style servers, while being technically independent and forward-looking.

---

## 2. Design Pillars

These five pillars guide every design decision in REQ. They represent the non-negotiable core values that differentiate this game from modern MMOs.

**World is dangerous**  
Pulling incorrectly, over-aggroing, or traveling unprepared causes wipes.

**Grouping matters**  
XP efficiency, crowd control, and dungeon pacing reward parties.

**Progression feels earned**  
Gear upgrades, spell tiers, and faction standing are long-term goals.

**Exploration and knowledge**  
Discovering locations, spawns, and quest chains is valuable.

**Authoritative multiplayer**  
Server decides truth; client is presentation.

---

## 3. Core Gameplay Loop

This loop defines the moment-to-moment and session-to-session rhythm of play. It's designed to create a satisfying cycle of preparation, risk, reward, and recovery.

**Prepare in town**  
Sell, repair, bank, train, craft, form groups.

**Travel to hunting grounds**  
Overworld navigation with escalating risk.

**Engage camps / dungeons**  
Pull → fight → loot → manage resources → repeat.

**Return / recover**  
Turn in quests, upgrade gear, train new abilities, save progress.

---

## 4. Player Experience Goals

These milestones shape the intended player journey from novice to veteran, ensuring each level band provides distinct experiences and challenges.

**Levels 1–10:** Learn roles in safer zones, find friends.

**Levels 10–30:** Class identity locks in; group camps become mandatory.

**Levels 30+:** Raid-style encounters, faction gates, longer dungeons.

---

## 5. World & Zones

The world structure defines how players experience space, travel, and danger escalation. It balances exploration with technical zone management.

### 5.1 World Structure

Multiple seamless regions broken into server zones.

Each zone: outdoor spaces + points-of-interest + 1–2 dungeons.

Zones have level bands and faction themes.

### 5.2 Travel

On foot initially; limited transport (boats/airships) added later.

Bind points / recall spells are rare utilities, not universal.

---

## 6. Races & Classes (Original)

Race and class combinations create player identity and strategic trade-offs. This section documents all playable options and their restrictions, inspired by classic EverQuest design.

### 6.1 Races

For each race, **Allowed Classes** indicates the class options.

**Human**  
Most versatile "baseline" race. Widest class access, neutral starting options. Few standout racials, but no major penalties.  
**Allowed classes:** Warrior, Cleric, Paladin, Ranger, Shadow Knight, Druid, Monk, Bard, Rogue, Necromancer, Wizard, Magician, Enchanter

**Barbarian**  
Large northern humans, tough and physically inclined. Strong melee/shaman identity; limited caster/holy options.  
**Allowed classes:** Warrior, Rogue, Shaman, Beastlord

**Erudite**  
Scholarly Odus natives. Pure intellectual/magic bent; excellent casters and priests, almost no "fighter" paths.  
**Allowed classes:** Cleric, Shadow Knight, Necromancer, Wizard, Magician, Enchanter

**High Elf**  
Refined, strongly "good" aligned. Priest/caster focus; iconic holy and arcane archetypes.  
**Allowed classes:** Cleric, Paladin, Wizard, Magician, Enchanter

**Wood Elf**  
Forest-dwelling, agile archers/scouts. Nature and dexterity-leaning classes; no heavy holy/caster variety.  
**Allowed classes:** Warrior, Ranger, Druid, Bard, Rogue, Beastlord

**Half Elf**  
Mixed heritage generalists. Strong "adventurer" vibe: ranger/bard/rogue/druid, decent flexibility.  
**Allowed classes:** Warrior, Paladin, Ranger, Druid, Bard, Rogue

**Dark Elf**  
"Evil" aligned underground elves. Shadow, stealth, and dark magic—great for SK/necro/rogue/arcane.  
**Allowed classes:** Warrior, Shadow Knight, Rogue, Necromancer, Wizard, Magician, Enchanter

**Dwarf**  
Stout, hardy "good" folk. Classic heavy-armor holy/melee blend; no "arcane scholar" routes.  
**Allowed classes:** Warrior, Paladin, Cleric, Rogue, Bard

**Halfling**  
Small, sneaky, community-minded. Rogue/druid/cleric options; surprisingly sturdy for size.  
**Allowed classes:** Warrior, Rogue, Cleric, Druid

**Gnome**  
Tinkerers and intellectuals. Arcane + rogue + necro tools; small-race utility, no holy knights.  
**Allowed classes:** Warrior, Rogue, Necromancer, Wizard, Magician, Enchanter

**Ogre**  
Huge, brutal "evil" giants. Top-tier melee and shaman/knight paths; famously strong physical race.  
**Allowed classes:** Warrior, Shadow Knight, Shaman, Rogue, Beastlord

**Troll**  
Regenerating swamp brutes. Melee, shadow, shaman; great sustain from racial regen.  
**Allowed classes:** Warrior, Shadow Knight, Shaman, Rogue, Bard, Beastlord

**Iksar (Kunark)**  
Reptilian empire survivors. Natural armor/regen flavor, strong monk/shaman/SK/necro identity.  
**Allowed classes:** Warrior, Shadow Knight, Shaman, Monk, Rogue, Necromancer, Beastlord

**Vah Shir (Luclin)**  
Feline Luclin natives. Martial + spiritual hybrid race introduced in Luclin.  
**Allowed classes:** Warrior, Shaman, Rogue, Bard, Beastlord

### 6.2 Classes

Each class fills a specific role in group composition. No class is redundant; each brings unique utility.

**Warrior** – Pure tank/melee. No spells; best defensive scaling.

**Cleric** – Premier healer with heavy buffs and undead control.

**Paladin** – Holy knight tank/hybrid; heals, stuns, undead nukes.

**Ranger** – Nature fighter hybrid; tracking, bows, snares, light heals.

**Shadow Knight (SK)** – Dark knight tank/hybrid; lifetaps, fear, aggro magic.

**Druid** – Nature priest; heals, ports, snares, DoTs, animal/plant control.

**Monk** – Martial DPS/puller; feign death, high avoidance.

**Bard** – Song-based support; pulling, crowd control, mana/health regen songs.

**Rogue** – Stealth melee DPS; backstab, lockpicking, traps.

**Shaman** – Tribal priest; slow, buffs, heals, strong DoTs.

**Necromancer** – Death mage; pets, DoTs, fear kiting, mana/health tricks.

**Wizard** – Burst nuker + evac/ports; top spike damage.

**Magician (Mage)** – Pet summoner; sustained DPS, summoned gear.

**Enchanter** – Crowd-control king; mez/charm, haste, mana support.

**Beastlord (Luclin)** – Melee + pet + shaman-style support hybrid.

**Note:** An "Arcanist" archetype (nukes + utility) can be used as a collective design label for Wizard / Magician / Enchanter if needed.

### 6.3 Class Identity Rules

Each class has 1–2 signature mechanics.

Every class contributes to groups in a real way.

### 6.3 Racial Stats and Trait Framework

REQ uses seven core attributes:

- Strength
- Stamina
- Agility
- Dexterity
- Intelligence
- Wisdom
- Charisma

For clarity, the table below shows adjustments relative to a baseline of 50 in every attribute. The data layer stores final numbers per race, but the gameplay intent follows this pattern.

Race	    STR	STA	AGI	DEX	INT	WIS	CHA	Key traits and notes
Human	    0	0	0	0	0	0	0	Baseline race, broad class access, neutral starting factions.
Barbarian	+10	+10	-5	0	-10	0	-5	High physical stats, modest cold resistance, tall body size, slightly clumsy casters.
Erudite	    -10	-5	-5	0	+15	+5	0	Premier intellect race, strong Arcane resist focus, fragile early melee.
High Elf	-10	-5	0	0	+10	0	+5	Strong mana and spell crit identity, favored by arcane and holy casters.
Wood Elf	-5	-5	+10	+5	0	+5	0	Nimble archers, strong trackers, light armor skirmishers.
Half Elf	0	0	+5	+5	0	0	0	Flexible midline stats, good at hybrid paths and generalist roles.
Dark Elf	-5	-5	0	+5	+10	0	0	Strong in shadow magic, night vision, bonus to stealth and Arcane resist.
Dwarf	    +5	+10	-5	0	-5	0	0	Very hardy, bonus poison and disease resist, small height, solid tank identity.
Halfling	-5	0	+10	+5	0	+5	0	Small evasive skirmishers, bonuses to sneak and hide, strong food themed flavor.
Gnome	    -5	0	0	0	+10	0	0	Tinkering and spellcasting focus, small size, good with devices and pets.
Ogre	    +15	+10	-10	0	-10	0	-5	Top tier melee stats, high frontal stun resistance, very large model and reach.
Troll	    +10	+5	-5	0	-10	0	-5	Innate health regeneration, strong poison resist, swamp survival theme.
Iksar	    0	+5	+5	0	0	0	0	Natural armor and regeneration, slight penalties from gear availability early on.
Vah Shir	+5	+5	+5	0	0	0	0	Agile feline warriors, bonuses to safe fall, tracking, and light melee.

Racial traits

In addition to stat shifts, each race has a small set of passive traits that can be implemented as tags in data:

- Size tag (small, medium, large) for hit box and camera handling.
- Vision tag (normal, low light, infravision) for how dark zones feel.
- Regen tag (normal, improved, strong) for out of combat health recovery.
- Special hooks such as front arc stun resistance (Ogre), strong poison resist (Dwarf, Troll), or mild swim bonus (Iksar).

Final numeric values and trait magnitudes are defined in race_data.json so that balance passes do not require design rewrites.

### 6.4 Class Primary Stats and Resources

Each class leans on a small set of primary attributes and one or more resource pools. This section summarizes the intended focus for itemization and tuning.

Class	        Main role	                    P   rimary stats	                    Resource focus	                                Armor and weapon notes
Warrior	        Defensive tank	                    Stamina, Strength, Agility	        Endurance for combat arts, high health	        Plate, shields, all melee weapons
Cleric	        Primary healer and buffer	        Wisdom, Stamina	                    Mana for spells, light Endurance	            Plate, blunt weapons, shields
Paladin	        Holy tank hybrid	                Stamina, Strength, Wisdom	        Mana and Endurance mix	                        Plate, shields, blunt and slashing
Ranger	        Melee and bow hybrid	            Agility, Dexterity, Wisdom	        Endurance for arts, Mana for utility	        Chain, bows, light blades
Shadow          Knight	Dark tank hybrid	        Stamina, Strength, Intelligence	    Mana and Endurance mix	                        Plate, two handed blades, shields
Druid	        Nature priest	                    Wisdom, Intelligence	            Mana	                                        Leather, staves, scimitars
Monk	        Martial DPS and puller	            Agility, Stamina, Dexterity	        Endurance heavy kit, light Mana	                Leather, fist weapons, staves
Bard	        Song based support	                Charisma, Dexterity, Stamina	    Endurance and light Mana	                    Chain, instruments, light blades
Rogue	        Stealth melee DPS	                Dexterity, Agility, Strength	    Endurance heavy kit	                            Chain, piercers, thrown weapons
Shaman	        Debuff and buff priest	            Wisdom, Stamina	                    Mana	                                        Chain, blunt weapons, spears
Necromancer	    Damage over time and pet caster	    Intelligence, Stamina	            Mana	                                        Cloth, staves, daggers
Wizard	        Burst nuker	                        Intelligence, Dexterity	            Mana	                                        Cloth, staves, daggers
Magician	    Pet summoner	                    Intelligence, Stamina	            Mana	                                        Cloth, staves, daggers
Enchanter	    Crowd control and mana support	    Intelligence, Charisma	            Mana	                                        Cloth, staves, daggers
Beastlord	    Melee and pet hybrid	            Stamina, Agility, Wisdom	        Endurance and Mana	                            Leather, fist and light weapons

Guiding principles:

- Casters and priests seek Int or Wis plus enough Stamina to survive.
- Tanks push Stamina and Armor Class, with Strength as secondary for threat and damage.
- Rogues, rangers, and monks lean heavily on Agility and Dexterity for hit rate and avoidance.
- Hybrids use both Mana and Endurance, so they value primary casting stat and physical stats at the same time.

### 6.5 Derived Stats and Formulas Classic EverQuest aligned

REQ uses primary attributes, class, race, and level to derive combat stats. The intent is to mirror classic EverQuest behavior as seen on early live servers and classic emulator projects, not to invent a new system.

This section describes the shape of the formulas so that server code can track classic behavior while still allowing tuning by data.

### 6.5.1 Hit points

Community research on classic EverQuest indicates that:

- Hit points per point of Stamina depend on class and level, and increase as level rises.
- Stamina gains above a high threshold (for example 255) produce only partial gains.

REQ adopts the same ideas in a data driven way.

Class categories

Each class belongs to a hit point category:

- Tank classes Warrior, Shadow Knight, Paladin
- Melee classes Monk, Ranger, Rogue, Bard, Beastlord
- Priest classes Cleric, Druid, Shaman
- Pure caster classes Wizard, Magician, Enchanter, Necromancer

Each category defines:

- Base hit points gained per level
- Hit points gained from Stamina per level
- A soft cap value for Stamina, with reduced gains beyond that cap

At a high level:

- Tanks gain the largest base hit points per level and the best returns from Stamina.
- Melee and hybrids gain moderate base hit points and moderate Stamina returns.
- Priests gain less base hit points but still benefit from Stamina for survivability.
- Pure casters gain the least base hit points and the weakest Stamina conversion.

Actual numbers are stored in a data table, for example hp_class_factors.json, so that emulator operators can tune them to match classic values or a specific era.

### 6.5.2 Mana

Classic research shows that mana for casters and priests scales with level and with a primary casting stat, usually Intelligence or Wisdom, with roughly one extra mana per level for each five points of that stat up to a soft cap, and reduced returns beyond that cap. 

REQ models this in a class and stat driven way.

Casting stats

- Intelligence is the main casting stat for int casters Wizard, Magician, Enchanter, Necromancer.
- Wisdom is the main casting stat for priests Cleric, Druid, Shaman and for Wisdom based hybrids such as Paladin and Beastlord.

Class mana factors

Each casting class defines:

- A base mana per level value.
- A mana per point of primary casting stat value, which scales with level.
- A soft cap on the casting stat (for example 200) after which returns per point are reduced.

In broad terms:

- Pure casters have the highest mana per level and the strongest benefit from Intelligence.
- Priests have slightly less total mana but still gain strongly from Wisdom.
- Hybrids have lower base mana and a weaker conversion factor, but still scale with their casting stat.
- Classes that do not cast spells either have no mana bar or only a very small pool for discipline like effects.

As with hit points, the exact multipliers are defined in data so that they can mimic known classic era formulas or a chosen emulator baseline.

### 6.5.3 Endurance

Classic EverQuest did not expose an endurance bar at launch, but modern EverQuest and EQEmu use an endurance like resource for melee disciplines. 
EQEmulator

REQ includes endurance as a dedicated resource for melee and hybrid abilities:

- Base endurance per level is defined per class category, with tanks and pure melee at the top.
- Stamina increases endurance more than other stats.
- Certain AA and item effects can increase maximum endurance or regeneration rate.

Endurance formulas are intentionally simple and linear so that they are easy to balance during testing.

### 6.5.4 Armor class and avoidance

Classic EverQuest splits defensive value across armor class (mitigation) and avoidance. Exact formulas on live are complex and were never fully documented, but community and emulator work shows that armor value from gear, level, and agility all matter. 

REQ follows that pattern:

- Mitigation AC
    - Comes mainly from worn armor, shields, and defensive buffs.
    - Gains a smaller contribution from level and a marginal contribution from Stamina.
- Avoidance AC
    - Comes mainly from Agility, Dexterity, and level.
    - Can be modified by certain buffs, debuffs, and items.

The combined AC value is stored and used server side to determine incoming melee damage and hit chance. The client can display a simplified number that matches classic EverQuest behavior.

### 6.5.5 Offensive ratings

Melee attack and spell offense use classic style ideas:

- Melee attack rating
    - Based on level, weapon skill, Strength, and class category.
    - Determines the chance to hit and to land higher damage rolls.
- Ranged attack rating
    - Based on level, archery or throwing skill, Dexterity, and class.
- Spell offensive checks
    - Use level, skill in the relevant spell school, and the resist system defined in section 17.3.

Exact equations for attack rating are in server code and tuned against classic logs and emulator behavior so that hit rates, miss rates, and crit rates land in familiar ranges.

---

## 7. Combat System

Combat is the heart of REQ's gameplay. This section defines the server-authoritative, tab-target system that supports tactical group play and role interdependency.

**Model:** Tab-target, cooldown + cast-time based, server authoritative.

**Resource Types:** Health, Mana, Endurance (class-specific).

**Threat:** Server-tracked hate table.

**Avoidance / Resists:** Percentage-based with level and gear scaling.

**Global Cooldown:** ~1.5s for instant abilities.

**Cast Interruptions:** From movement, damage threshold, or control effects.

Crowd control tools include root, mez, fear, slow, stun, silence equivalents with class-specific rules.

**Why GAS?**  
Unreal's Gameplay Ability System (GAS) fits RPG/MMO abilities and makes cooldowns, tags, buffs, and replication structured and maintainable.

### 7.1 Combat Model Overview

Tab-target, server authoritative.

Auto-attack baseline with ability/spell weaving.

Longer TTK (time-to-kill) than modern ARPGs to support group roles.

### 7.2 Hate / Threat System

The threat system ensures tanks can control enemy targeting while rewarding skilled play and punishing mistakes.

Each hostile NPC maintains a hate list: a ranked table of attackers prioritized by hate value.

**Data structure:** `HateTable: map<EntityID, HateValue>`

**Hate sources:**

- Melee damage: `hate += damage * meleeHateScalar`
- Spell damage: `hate += damage * spellHateScalar`
- Heals/buffs on targets of enemies: hate split across hostile list
- "Snap hate" abilities: instant fixed hate bonus

**Tank tools:**

- **Taunt:** Set your hate to TopHate + 1 on success.
- **Threat skills:** Add hate without damage, medium cooldown.

### 7.3 Aggro Acquisition Types

**Proximity aggro:** Entering NPC awareness radius.

**Social aggro:** Nearby allies assist.

**Damage aggro:** Being hit.

**Spell aggro:** High-hate debuffs and control effects.

Per-NPC archetype scalars tune each type.

### 7.4 Pulling & Encounter Control

Pulling mechanics support the classic "camp and pull" playstyle that defines old-school MMO dungeon crawling.

Supports classic camp-based pulling:

- Line-of-sight pulls
- Body pulls (edge of aggro radius)
- Split pulls using CC, snares, feign-drop, or pacify-style tools

**Zone rules:**

- Soft leash outdoors
- Hard leash in dungeons

### 7.5 Attack Resolution (Per Swing / Hit)

1. Hit check (attacker accuracy vs defender avoidance)
2. Mitigation (armor / resist / shield)
3. Crit / special roll
4. Proc roll (on-hit effects)

### 7.6 Crowd Control Rules

CC spells apply tags: `State.Mez`, `State.Root`, `State.Fear`, etc.

**Mez:** Breaks on direct damage.

**Root:** Periodic break checks.

**Fear:** Breaks on high burst or duration timer.

### 7.7 Out-of-Combat Recovery

Downtime is a deliberate design choice that creates social opportunities, resource management, and pacing between encounters.

Health regen slow baseline; faster when sitting/resting.

Mana regen tied to Spirit/Intelligence and "rest state."

Food/drink provide mild bonuses.

Downtime pacing is deliberate to encourage grouping and safe camps.

---

## 8. Social Systems

Social interaction is a core pillar. These systems facilitate cooperation, communication, and community building.

### 8.1 Grouping (Party of 6)

Shared XP with role bonuses.

Loot rules: FFA, Round-Robin, Need/Greed.

Group UI: resource bars and status effects.

### 8.2 Raids (Optional for v1.0)

Target size: 18–36 players.

Raid roles and assist markers.

Likely post-vertical-slice feature.

### 8.3 Chat

Channels: Say, Shout, Group, Guild, Tell, System.

Persistent chat tabs and filters.

---

## 9. Progression

Character advancement is a long-term journey. This section outlines the systems that reward investment and skill development.

### 9.1 Levels

Levels 1–60 (scalable; cap can be expanded later).

### 9.2 XP Sources

- Kills
- Quest turn-ins
- Discoveries / exploration milestones

### 9.3 Skill System

Automatic skill ups through use (generic skill categories).

Training NPCs unlock new tiers rather than raw point allocation.

### 9.4 Gear Tiers

Common → Uncommon → Rare → Epic → Mythic (names adjustable).

Stats, resistances, and proc effects.

### 9.5 Experience Curve and Group Distribution Classic EverQuest aligned

REQ aims to reproduce the feel of classic EverQuest experience gain, including slow leveling at higher levels, zone experience modifiers, and group experience behavior.

Most numbers come from community reverse engineering of classic EverQuest and EQEmu based projects. 

### 9.5.1 Experience per level

Key properties of the classic curve:

- Total experience required per level grows faster than linear, close to a quadratic curve with level. 
- Certain levels, commonly known as hell levels, require significantly more experience than adjacent levels.
- Later eras such as Luclin smoothed some of these bumps, but classic servers keep them. 

REQ mirrors this:

- A base function gives experience required per level, roughly quadratic in level.
- A separate data driven multiplier table marks any hell levels (for example 30, 35, 40, 45, 51 to 54) and applies extra required experience at those levels.
- The curve is stored as data or as a small function in one place so that emulator operators can choose whether to keep classic hell levels or use a smoothed progression.

This allows close emulation of classic behavior while still supporting optional Luclin style smoothing in later rulesets.

### 9.5.2 Experience from kills

Classic research and EQEmu work show that experience from a single kill depends primarily on:

- Level of the mob relative to the player.
- A base per level experience value that roughly follows level squared.
- The Zone Experience Modifier (ZEM), which makes some zones faster or slower. 

REQ uses the same components:

- Base mob experience value is a function of mob level, similar in shape to level squared times a constant. 
- A zone modifier multiplies that value, with outdoor fields near 1.0 and dungeons or high risk zones above 1.0.
- Very low level mobs that con green or trivial to the player give strongly reduced experience or none, with several discrete steps rather than a smooth curve. 

Zone experience modifiers are defined in zone_data.json, so operators can match known classic ZEM lists or their preferred custom values.

### 9.5.3 Race and class experience penalties

In classic EverQuest certain races and classes had experience penalties or bonuses, and in some cases these stacked, such as Troll Shadow Knight combinations. 

REQ keeps this behavior as a ruleset option:

- Each race and each class can define an experience modifier, positive or negative, in races.json and classes.json.
- The server multiplies the base experience gain for a character by the combined race and class modifier.
- These modifiers also affect how experience is split in groups, described below.

For classic accurate rulesets, these values can be set to match known penalty combinations. For modern friendly rulesets, the modifiers can be set to neutral.

### 9.5.4 Group experience sharing

Classic EverQuest shares experience among group members in a way that takes into account total experience and class penalties, rather than simply splitting by level. Emulator work and developer commentary indicate that this was done in order to keep characters with penalties leveling at similar rates to others in the same group. 

REQ follows this design:

- Experience from a kill is allocated among group members proportional to each member total experience value, adjusted by that member race and class experience modifiers.
- Members with experience penalties take a larger share of the group experience pool. Members without penalties take a smaller share. The result is that the group tends to stay at roughly the same level over time.
- Extremely low level members relative to the highest member receive reduced experience or none, based on a level range rule.

In addition, REQ supports a group bonus to encourage grouping:

- A base group bonus factor increases total experience as group size rises.
- Classic sources differ on exact numbers and vary by era, but the pattern is that full groups gain noticeably more total experience per kill than solo characters, while still splitting that experience. 

The exact bonus per member is defined in world_rules.json so operators can tune it per ruleset.

### 9.5.5 Quest experience

Quest experience rewards in classic EverQuest are often lower yield per turn in than raw grinding, especially at higher levels, but still follow the same underlying experience curve. 

REQ uses a simple but flexible quest experience model:

- Quest rewards are expressed as a percentage of the current level or as a fixed experience value, with optional caps by level.
- Server code converts that percentage into raw experience using the same per level requirements defined in this section.
- This keeps quest experience and kill experience aligned and allows operators to tune how generous quests are compared to grinding.

All of the above values are data driven and exposed in configuration, so that classic strict, progression friendly, and experimental rulesets can share the same code but differ in numbers.

### 9.6 Deity Alignment System

REQ models deity choice as a mechanical field plus content hook, similar to classic EverQuest. The core server does not hard code any pantheon logic. Instead it exposes a small set of behaviors that content and data drive.

Storage

Each character has:

- A deity identifier field.
- An alignment tag that can be used for broad grouping, such as good, neutral, evil.

The identifier is an opaque value from the point of view of the core server. Actual names and lore live entirely in data and content.

System level uses

Baseline uses that match EverQuest style behavior:

- Faction offsets
    - Starting faction can vary by deity as well as race and class.
    - Some temples, fanatics, or priesthoods treat deity aligned characters as allies or enemies.
- Item and spell restrictions
    - Items and click effects can require or exclude certain deity identifiers.
    - This is implemented as a simple list of allowed or banned deity ids on the item record.
- Quest checks
    - Quests can branch or lock based on deity, for example rival priesthood lines or deity trials.

Out of scope for vertical slice

- Pantheon lore and specific names.
- Complex deity mechanics such as worship events or divine meters.

For the vertical slice, it is enough that:

- Deity is stored on character creation.
- Faction and item systems can look at that field when needed.
- Content authors can mirror, one for one, deity based behavior from original EverQuest data if they choose.

---

## 10. Items, Loot, Economy

The economy is player-driven with meaningful item scarcity and trading. This section defines item types, loot mechanics, and economic design.

### 10.1 Items

- Stackable consumables
- Equipment
- Quest items
- Crafting materials

**Bind rules:**

- Bind-on-pickup
- Bind-on-equip
- Unbound

### 10.2 Loot

Per-NPC loot tables with:

- Guaranteed slots
- Weighted random pools
- Optional rare drop "pity counter"

### 10.3 Economy

NPC vendors as gold sinks + basic supply.

Player trading is the primary economy driver.

---

## 11. Crafting

Crafting offers meaningful item creation with risk and reward. Players can specialize in professions that support the broader economy.

### 11.1 Stations

- Forge
- Loom
- Alchemy bench
- Cooking fire
- Carving table

### 11.2 Recipes

Require materials + station tag + skill minimum.

### 11.3 Failure

Partial loss by tier unless protected by tools.

---

## 12. Death & Recovery

Death has meaningful consequences that reinforce the dangerous world pillar without being punitive to the point of frustration.

Death creates a recoverable corpse/container with items (configurable rules).

XP debt or minor XP loss (tunable).

Respawn at bind point or healer NPC.

Resurrection spells recover a percentage of losses.

---

## 13. UX / UI Summary

The UI supports classic MMO information density while maintaining modern usability standards. This overview sets expectations before the detailed widget spec.

**Core MMO-style HUD:**

- Player bars
- Target frame
- Hotbars / abilities
- Buff/debuff bars
- Chat
- Group frame

**Primary menus:**

- Inventory / equipment
- Spellbook / abilities
- Quest journal
- Map
- Social (guild/friends)
- Options

**UMG guidance:**  
Use event-driven UMG; avoid Tick/polling where possible for performance.

---

### 13.1 Modern Quality-of-Life & Server-Level Options

Although REQ is built around a classic, information-dense MMO HUD, the client exposes a set of opt-in, server-configurable quality-of-life (QoL) features. The intent is to let each server define its own "strict classic" vs "modern convenience" balance.

**QoL principles:**

- Defaults are conservative and classic-leaning.
- All QoL options are driven by server config and advertised in the server selection UI.
- Players should never need external tools or UI mods to understand basic state.

**Example QoL features (toggleable per server):**

**Targeting con/faction visuals**  
Target frames and world-space outlines can pulse or glow using color schemes mapped to either:

- Difficulty / level ("consider") bands (e.g., grey/green/light blue/white/yellow/red), and/or
- Faction tier (e.g., Ally / Warmly / Amiable / Indifferent / Apprehensive / Dubious / Threatening / Scowls).

Intensity and animation (subtle pulse vs strong outline) are tunable and can be reduced or disabled for a more classic feel.

**Improved selection feedback**  
- Clear, readable target brackets in 3D space.
- Optional "mouseover" highlight with faction/con text in the tooltip.

**Accessibility options**  
- Colorblind-safe palettes for con/faction colors.
- UI scaling and font size controls.

**Information overlays (optional)**  
- Minimalistic floating combat text (damage, resists, interrupts).
- Optional cast bars above hostile casters.

Each server can define a **QoL preset** (e.g., *Strict Classic*, *Classic + Readability*, *Modern Assist*) that controls which of these systems are active. These presets are primarily implemented as data/config on the server side so that ruleset changes do not require client code changes.

---

## 14. Technical Architecture (Client / Server)

REQ uses a hybrid architecture: a **Unreal Engine 5.7 client** for presentation and input, connected to a set of **pure C++ backend servers** (Login, World, Zone, Chat) built on **Boost.Asio**. The servers are fully engine-independent and authoritative; the client is a thin, non-authoritative view and input layer.

### 14.1 High-Level Topology

- **Client (UE5.7):**
  - Renders the world, animations, VFX, UI.
  - Handles local input, basic prediction, and interpolation.
  - Implements client-side ability logic and presentation (GAS-style patterns), but never decides authoritative outcomes.
- **Backend Servers (C++ / Boost.Asio):**
  - Login, World, Zone, and Chat servers are standalone C++ processes.
  - All combat, movement, aggro, AI, and state changes are decided here.
  - Servers communicate with clients using a custom, versioned protocol over Boost.Asio.

This separation allows the backend to scale and deploy independently of Unreal, while the client stays focused on moment-to-moment gameplay feel.

### 14.2 Client (UE5.7)

The UE client is structured using standard Unreal gameplay concepts, but treats the network as the source of truth:

- Uses systems analogous to:
  - **GameMode / Rules Layer:** Local representation of rules/state expected from the server.
  - **GameState / PlayerState:** Local mirrors of server-authoritative state.
- Handles:
  - **Input capture:** Movement, targeting, hotbar use, interaction.
  - **Prediction & interpolation:** Smooth movement and animation based on server snapshots.
  - **Ability presentation:** Cast bars, cooldown visuals, floating combat text, buff/debuff icons.
  - **UI & QoL:** Target outlines, con/faction colors, tooltips, chat windows, etc.

The client never makes final decisions about:
- Hit/miss, damage amounts, resist checks.
- XP gain, loot rolls, faction hits.
- Authoritative positions or teleportation.

Those are always confirmed by the server and then reflected on the client.

### 14.3 Backend Servers (C++ / Boost.Asio)

All backend services are engine-agnostic C++ applications using **Boost.Asio** for high-performance, asynchronous networking:

- **Login Server**
  - Account authentication and registration.
  - Issues session tokens on successful login.
  - Provides the list of available worlds and their statuses.

- **World Server**
  - Manages worlds/realms and per-account character lists.
  - Handles character creation, deletion, and selection.
  - Issues short-lived **handoff tokens** and connection info for the appropriate Zone server when a character enters the game.

- **Zone Servers**
  - Each zone (or group of zones) runs as one or more Zone server processes.
  - Authoritative simulation:
    - Positions, movement, collision resolution.
    - Combat resolution (melee, spells, procs).
    - Hate/aggro, leashing, pathing.
    - NPC AI states and scripting.
  - Publishes periodic state snapshots and event messages to connected clients.
  - Optional zone process manager: When configured, the World server can auto-launch and monitor Zone server processes for its zones, instead of relying on external process managers.

- **Chat Server (optional)**
  - Dedicated process for global, zone, guild, party channels and direct tells.
  - Decouples chat load from gameplay simulation.

Key properties:
- All network IO is async via Boost.Asio `io_context` loops.
- Servers are designed to run on **Linux or Windows** without Unreal runtime dependencies.
- Scaling out is done by adding more Zone server processes and/or shard Worlds as needed.

### 14.4 Client Networking Layer

The UE client includes a thin networking module responsible for:

- Establishing TCP (and optionally UDP) connections to the backend.
- Serializing and deserializing messages using the **REQ custom protocol**.
- Managing reconnects and **handoffs** between Login → World → Zone servers.
- Queuing outgoing input intents (movement, abilities, chat) and applying incoming authoritative updates.

Design principles:

- The networking layer is **engine-integrated but protocol-agnostic**:
  - Implemented in UE C++ (and optionally exposed to Blueprints).
  - Talks directly to the Boost.Asio servers using raw sockets.
- The protocol is **versioned** and **binary**, designed for:
  - Low overhead per message.
  - Clear separation of message types (login, world list, zone state, chat, etc.).
  - Future extensibility (additional fields, new message types).

### 14.5 Session & Handoff Flow (Login → World → Zone)

High-level connection flow for a player:

1. **Client → Login Server**
   - Connects to Login.
   - Sends credentials or registration request.
   - Receives:
     - Auth result.
     - Account-wide **session token**.
     - World list (with basic population/latency info).

2. **Client → World Server**
   - Chooses a world from the list.
   - Connects to the World server using the session token.
   - Receives:
     - Character list (name/class/level/last zone).
     - Ruleset/QoL profile for that world.

3. **Client → Zone Server**
   - After selecting or creating a character, the World server:
     - Chooses the appropriate Zone server.
     - Issues a **zone handoff token** and connection address.
   - Client connects to the Zone server with this token.
   - Zone server:
     - Validates the token.
     - Spawns the character in the zone.
     - Begins streaming state updates.

4. **In-Game**
   - Client sends input intents: movement, targeting, abilities, chat.
   - Zone server sends authoritative updates: positions, HP/MP changes, buffs/debuffs, NPC spawns/deaths, loot, faction hits.

5. **Transfers & Camp**
   - Zoning to another area triggers a new Zone server handoff with a fresh token.
   - Camping out returns the player to the character select flow via the World server.

This explicit handoff model mirrors classic MMO architectures while being modernized via Boost.Asio and a custom protocol tailored for REQ.

### 14.6 Zone Server Topology & Auto-Launch

REQ supports two main deployment modes for Zone servers. The choice is **per world** and controlled via JSON config (`world_config.json`).

#### 14.6.1 Manual Mode (External Orchestration)

Default behavior mirrors traditional emulator stacks:

- Zone servers are started and monitored by external tools (systemd, Docker, Kubernetes, Windows services, etc.).
- `world_config.json` specifies a `zones` list with:
  - `zone_id`
  - `zone_name`
  - `host`
  - `port`
- The World server:
  - Treats these as already-running services.
  - Routes players to the appropriate Zone server based on `zone_id`.

This mode is ideal for production setups where operations teams prefer to manage processes and scaling themselves.

#### 14.6.2 Auto-Launch Mode (World-Managed Zones)

For simpler setups (local dev, small community servers), the World server can manage Zone server processes directly.

Enabled via `world_config.json`:

jsonc
{
  "world_id": 1,
  "world_name": "CazicThule",
  "auto_launch_zones": true,
  "zones": [
    {
      "zone_id": 10,
      "zone_name": "Freeport",
      "executable_path": "./zone_server",
      "args": ["--world_id=1", "--zone_id=10", "--port=7000"]
    }
  ]
}

Behavior in auto-launch mode:

On startup, the World server:

Iterates zones[] from config.

Spawns a child process per zone using executable_path + args.

Tracks child process status:

Marks zones as online/offline for world/zone lists.

Optionally respawns crashed zones if configured ("auto_restart_zones": true).

Exposes zone status to:

The Login/World UI (population, online/offline).

Admin tools (for remote restart/disable).

Design goal: keep small/home servers easy to run (“start world, everything comes up”) while still supporting serious deployments that prefer external orchestration.

### 14.7 Player Movement Authority & Anti-Cheat

REQ uses a fully **server-authoritative movement model**. The client is never trusted for final positions or velocities.

#### 14.7.1 Input-Intent Model

- The client sends **input intents**, not raw positions:
  - Move vector / strafe / jump.
  - Facing changes and pitch/yaw updates.
- Each input packet is tagged with:
  - A **sequence number**.
  - A **client timestamp** (for lag compensation and prediction).

The Zone server:

- Integrates movement using server-side physics and collision.
- Applies:
  - Speed caps and acceleration limits (per class/buff).
  - Slope and step height limits from the zone’s navmesh/collision.
  - Fall damage and terminal velocity rules.

The server periodically sends back **authoritative snapshots** (position, velocity, orientation). Clients use local prediction and interpolation between these but always reconcile to server truth.

#### 14.7.2 Anti-Cheat Checks

The movement pipeline is designed to catch common cheats:

- **Speed/teleport detection**
  - Reject or clamp deltas that exceed speed/fall constraints.
  - Flag repeated violations for GM review or auto-kick thresholds.
- **No-clip prevention**
  - Reject positions inside solid geometry or outside navmesh.
  - Snap offenders back to last valid location.
- **Path sanity**
  - For ground-bound movement, ensure samples stay on navmesh and within allowed slope.
- **Sequence / replay protection**
  - Require monotonically increasing sequence numbers.
  - Drop out-of-order or replayed movement packets.
- **Rate limiting**
  - Cap movement input rate per tick to avoid flood/spam exploits.

#### 14.7.3 Enforcement Levels

Enforcement is configurable per world:

- **Lenient**: Always snap/correct, mostly log-only.
- **Normal**: Snap, log, and disconnect on sustained suspicious behavior.
- **Strict**: Lower tolerance for repeated violations, intended for competitive rulesets.

Core requirement: **movement is never client-authoritative**. All future systems (abilities, mounts, knockbacks, etc.) must respect this model.

---

## 15. Vertical Slice Milestones

Development is broken into four incremental milestones, each proving core systems before expanding scope.

### 15.1 Milestone 0 — Skeleton

Login → world select → character create/select → load into 1 zone.

Basic movement + replication.

### 15.2 Milestone 1 — First Camp

1 outdoor zone, 5 NPC types, loot + XP.

Targeting + combat + death loop.

### 15.3 Milestone 2 — Grouping

Party system, shared XP, group UI.

1 mini-dungeon.

### 15.4 Milestone 3 — Economy

Vendors

Loot rarity

Basic crafting

---

## 16. Faction & Reputation System (Detailed)

Faction creates dynamic NPC relationships and long-term strategic choices. Your actions with one group affect how others perceive you.

### 16.1 What Faction Is

Faction is a per-player reputation score with many NPC groups. NPC reactions, services, prices, quests, and attack-on-sight behavior derive from this value.

Players have standing with multiple factions; actions shift those standings (kills, quests, dialogue, events, assists).

### 16.2 Faction Tiers ("Con" Colors / Text)

NPCs show a tiered reaction string based on numeric reputation. Classic tiers:

- Ally
- Warmly
- Kindly
- Amiable
- Indifferent
- Apprehensive
- Dubious
- Threatening
- Scowls / Ready to attack

**Example default numeric mapping (tunable):**

| Tier         | Default Range     |
|--------------|-------------------|
| Ally         | 1100 to 2000      |
| Warmly       | 750 to 1099       |
| Kindly       | 500 to 749        |
| Amiable      | 100 to 499        |
| Indifferent  | 0 to 99           |
| Apprehensive | -100 to -499      |
| Dubious      | -500 to -749      |
| Threatening  | -750 to -1049     |
| Scowls       | -1050 to -2000    |

Tiers gate dialogue, vendor access, travel services, and KOS status.

### 16.3 How Faction Changes

Faction shifts come from:

- Kills (positive for enemies, negative for their allies)
- Quests / turn-ins
- Dialogue choices
- World events
- Assist actions (e.g., healing someone fighting a faction)

**Implementation:**

Each relevant event emits `(FactionID, Delta)` entries server-side.

Client receives summary updates for UI.

### 16.4 Multi-Faction Ecosystems

Zones usually have interlocking factions, for example:

- Civil faction (town/guards/vendors)
- Wild hostile faction (raiders, beasts, cults)
- One or two subfactions (merchant cartel, shrine order)

Raising one may lower another to create long-term choices.

### 16.5 Services Affected by Faction

**Aggression:** Threatening/Scowls = KOS; Apprehensive/Dubious = wary but non-KOS.

**Vendors:** Price multiplier by tier (e.g., Ally = 0.8×, Indifferent = 1.0×, Dubious = 1.3×).

**Quests:** Minimum/maximum tier gates.

**Travel:** Some handlers deny hostile players.

**Training:** Class mentors may require higher tiers (e.g., Kindly+).

### 16.6 Disguise / Illusion Modifiers (Optional)

Temporary effects can add a faction offset while active:

- Illusions (race/culture guise)
- Uniforms / banners
- Diplomatic tokens

These modify considered faction but not the underlying score.

---

## 17. Magic & Ability System (Detailed)

Magic is complex, skill-based, and interruptible. This section defines spellcasting mechanics, resist systems, and ability design patterns.

### 17.1 Spellcasting Fundamentals

Cast time + recovery time.

Global spell recovery (~2 seconds) separate from individual spell recasts.

Spell schools (e.g., Alteration, Conjuration, Evocation, Abjuration, Divination, Enchantment).

Skill checks influence fizzle chance; specialization can reduce mana cost.

### 17.2 Interruptions & Channeling

Taking melee hits or being pushed can interrupt a cast.

Interrupt check: `baseChance – ChannelingSkillScalar`

Extra penalties for multiple attackers or strong pushback.

Some classes gain windows of reduced or zero interruption as high-value utilities.

### 17.3 Resist & Save System

REQ follows an EverQuest style resist model. Resists make gear, buffs, and debuffs matter, and create variance in spell outcomes.

Resist stats

Every actor has the following resist attributes:

- Fire Resist
- Cold Resist
- Poison Resist
- Disease Resist
- Magic Resist

Additional combined or special resist types, such as chromatic style saves, can be added later but are not required for the vertical slice.

Resist types on spells and abilities

Each spell or ability entry in spells.json or abilities.json has:

- A DamageType field that describes what kind of damage it deals (physical, fire, cold, poison, disease, magic).
- A ResistType field that picks which resist stat is used for the saving throw, or None for effects that do not check resists.

Typical mappings that mirror classic EverQuest:

- Direct fire nukes: DamageType Fire, ResistType Fire.
- Cold nukes and snares: DamageType Cold, ResistType Cold.
- Poison or disease dots: DamageType Poison or Disease, ResistType Poison or Disease.
- Many mez, stun, charm, and pure magic nukes: DamageType Magic, ResistType Magic.
- Heals and most self buffs: ResistType None.

Save resolution

When a spell with a ResistType targets an actor, the server evaluates:

- Caster level and spell level.
- Target level.
- Target resist stat for that type.
- Any debuffs that lower that resist.
- Any flags that indicate the spell is easier or harder to resist than normal.

The result is one of:

- Full hit.
- Partial resist (for example half damage or shortened duration).
- Full resist.

Exact numeric formulas are data driven and can be tuned to approximate classic EverQuest results, including higher resist rates on red or yellow con targets.

Creature resist profiles

To match EverQuest style content, resist tendencies are defined on NPC templates, for example:

- Undead with low Fire and high Poison and Disease resists.
- Elementals with strong resists in their own element and weak resists in the opposing one.
- Animals with low Magic resists and varied body based resists.

NPC templates can inherit a base resist profile from a family or race, then apply overrides so that specific raid or named mobs have unique resist behavior.

### 17.4 Buff / Debuff Architecture

All long effects are GameplayEffects (if using GAS), tagged by category:

`Buff.Stat`, `Buff.Regen`, `Debuff.Slow`, `Debuff.ResistDown`, etc.

**Stacking rules:**

- Same line: overwrite by higher tier.
- Different lines: stack if tags allow.

### 17.5 Spell Roles by Archetype

**Healers:** Direct heals, HoTs, cures, mitigation buffs.

**Tanks:** Hate spells, mitigation, lifetaps (dark tanks).

**Controllers:** Mez/charm/root/fear/slow; high hate by design.

**Nukers:** Burst damage + evac/teleport utility.

**Pet casters:** Summoned companions with independent hate.

### 17.6 Mana Economy & Pacing

High-efficiency spells for sustain.

High-burst spells for emergencies.

Mana regen downtime is a pacing tool and encourages grouping.

### 17.7 Visibility & Counterplay

Spell cast bar visible to caster; optionally visible to target in PvP.

Main counters: interrupts, resists, CC breaks.

Some mobs have innate resist profiles (e.g., poison-resistant spirits).

### 17.8 Server Ability Engine and Client Parity

REQ’s magic and ability system is fully server-authoritative. The client presents abilities, tooltips, and visuals, but all real combat outcomes are decided by the backend servers.

Canonical definitions

Abilities and spells are defined in data files such as spells.json and abilities.json. Each entry contains:

- Identifiers: ability or spell ID, name, school, tags (damage, heal, crowd control, utility, etc.)
- Usage data: class and level requirements, resource costs, cast time, cooldown, global cooldown behavior, range and targeting rules
- Effect components: a list of individual effects such as direct damage, healing, damage-over-time, heal-over-time, buffs, debuffs, stat modifications, teleports, threat changes, summons, or script triggers
- Flags for PvP behavior, stacking rules, and special interactions (procs, criticals, partial resists, immunities)

Server-side ability engine

The server is the only place where spells and abilities are resolved. When a player attempts to use an ability, the server:

- Validates that the caster is allowed to use it:
    - Class, level, and specialization requirements
    - Cooldowns and global cooldown status
    - Resource checks (mana, endurance, charges, reagents)
    - Silence, stun, root, or other disabling conditions
- Validates targeting:
    - Allowed target types (self, ally, enemy, corpse, ground, area)
    - Range checks and line-of-sight rules
    - Zone, encounter, and PvP restrictions
- Executes the ability’s effect components against authoritative game state:
    - Rolls to hit or resist where appropriate
    - Applies damage, healing, and shields
    - Applies or removes buffs and debuffs
    - Adjusts threat and hate lists
    - Spawns or despawns pets, guardians, projectiles, or objects
    - Triggers secondary events or scripts where defined

The server then emits compact result messages to clients describing outcomes:
- Cast started, interrupted, or completed
- Hit versus miss, full versus partial resist
- Damage and healing values (possibly aggregated)
- New buffs and debuffs applied or removed, including durations and stacks

Client ability layer

The Unreal Engine client uses the same ability and spell IDs to:

- Populate hotbars, spellbooks, and tooltips
- Drive cast bars, resource bars, and cooldown display
- Trigger animation montages, particle systems, and audio

When a player activates an ability, the client sends an intent message to the server containing:

- Ability or spell ID
- Target information (target ID, ground position, or facing/area parameters)
- Optional local sequence number or timestamp for prediction

The client may predict casting locally for responsiveness (show a cast bar, start animations), but it always reconciles to server-confirmed outcomes and never applies real gameplay effects on its own.

Anti-cheat alignment

Because both movement and abilities are server-authoritative:

- Client-side manipulation of cast times, cooldowns, or ranges cannot directly change combat results
- All buff, debuff, and proc states exist on the server and are replicated outward, rather than being trusted from the client

This preserves the feel of an EverQuest-style spell system while ensuring that combat remains secure and consistent across all players.

### 17.9 Early Prototype Class Kits for Levels 1 to 10

For initial implementation and testing, REQ focuses on a small subset of the classic class list:

- Warrior
- Cleric
- Wizard
- Enchanter

The goal is to demonstrate tank, healer, damage, and crowd control roles in a way that feels like early EverQuest.

Warrior prototype kit
- Auto Attack
    - Always available melee swings.
    - Physical damage, no resist check.
- Taunt
    - Low level ability that attempts to set hate to slightly above the current top target.
    - Short cooldown, small Endurance cost.
- Kick or Bash
    - Short cooldown melee strike.
    - Physical damage, no resist check.
    - Bash variant requires shield.
- Defensive Discipline (early rank)
    - Short duration self buff that increases mitigation.
    - Long cooldown, Endurance based.

This is enough to let the warrior hold aggro and survive in early group content.

Cleric prototype kit
- Minor Healing
    - Fast, small direct heal.
    - No resist check.
- Light Healing
    - Larger, more efficient heal at slightly higher level.
- Smite
    - Low level direct damage spell.
    - Magic or Fire damage, uses Magic Resist.
    - May deal bonus damage to undead flagged targets.
- Armor type buff
    - Early rank defensive buff that increases Armor Class and possibly hit points.
- Cure line starter
    - Simple single type cure, such as Cure Poison or Cure Disease.

This mirrors the classic EverQuest flow where clerics gain basic heals, a small nuke, and early buffs very early in progression.

Wizard prototype kit
- Fire Bolt line starter
    - Fast, low damage fire nuke, Fire Resist.
- Frost line starter
    - Cold nuke that may apply a small snare or slow effect, Cold Resist.
- Magic based nuke
    - Pure magic nuke that uses Magic Resist.
    - Becomes the primary burst spell against targets with low Magic resists.
- Minor self rune or shield
    - Small self protection spell that absorbs some damage.

This gives a clear sense of wizard identity as a burst caster with strong element based damage.

Enchanter prototype kit
- Minor Magic Nuke
    - Low damage, low hate nuke that uses Magic Resist.
- Lull or Calm
    - Single target lull effect that reduces aggro radius, Magic Resist.
    - Used for splitting camps, matching classic EverQuest behavior.
- Mesmerize starter
    - Short duration single target mez, Magic Resist.
    - High hate on cast, breaks on damage.
- Root starter
    - Single target root, Magic Resist.
    - Holds the target in place with a chance to break on damage.
- Mana regeneration buff starter
    - Very early rank mana regeneration buff for one target.
    - Models the classic enchanter battery role.

Spells and abilities above are intended as thin slices of the full classic class kits, with naming and effects close to their EverQuest counterparts, but implemented as generic data entries.

---

### 18. NPC Archetypes and AI Behaviors

Goal: Provide a small set of reusable behaviors that cover classic camp and pull gameplay, without complex scripting for every creature.

### 18.1 NPC Archetype Taxonomy

Each NPC template declares an archetype field. Archetypes define default combat role, target selection, and resource usage.

Example archetypes:

- Melee trash
Basic melee bruisers. Close range, no spells. Used for filler pulls and ambient threats.
- Caster trash
Lightly armored spell users. Prefer to stand at range and use nukes or debuffs rather than melee.
- Healer
Support casters that can heal and buff allies. Low personal damage.
- Tank or brute
High health and armor. Slow movement and attack speed but heavy melee hits.
- Runner
Low or medium health NPCs that attempt to flee when injured past a threshold, often bringing adds.
- Boss and mini boss
Named enemies with increased health, damage, and additional scripted behaviors such as phases or add spawns.

Archetype selection drives default values for:

- Aggro radius and social radius
- Preferred distance to target
- Spell usage profile (none, light, heavy)
- Flee logic and thresholds
- Default resist profile and crowd control behavior

### 18.2 Behavior Flags and Parameters

Each NPC template can override behavior through a set of flags and numeric fields. Zone content authors set these in npc_templates.json and npc_spawns.json.

Key boolean flags:

- IsRoamer (moves along a path or random wander points)
- IsStatic (remains at spawn location unless in combat)
- IsSocial (assists nearby allies when they are damaged or aggroed)
- UsesRanged (prefers bows or spells at distance, only closes if cornered)
- CallsForHelp (attempts to alert nearby allies when under attack or near death)
- CanFlee (allowed to run at low health)
- ImmuneToMez / ImmuneToCharm / ImmuneToFear (for special targets and bosses)
- LeashToSpawn (returns to spawn area after a distance or time limit)

Important numeric parameters:

- AggroRadius
- SocialRadius
- FleeHealthPercent
- LeashRadius and LeashTimeout
- MaxChaseDistance
- PreferredRange (for ranged and caster archetypes)
- AssistDelay (time between ally aggro and assist decision)

Zone server AI uses these fields to control per tick behavior in a simple finite state machine:

Idle → Patrol → AggroChase → Combat → Flee or Reset

### 18.3 Spell and Ability Use For Caster Types

Caster and healer archetypes share a common spell usage model. Spells are grouped into logical lines in spells.json, for example:

- Opening debuff
- Main nuke
- Heal
- Root or snare
- Buffs

Basic rules:

- On first aggro, if a debuff line is available and target is in range, attempt one debuff cast before switching to nukes.
- During combat, alternate nukes with short pauses, while respecting global cooldown and mana.
- If healer archetype and an ally under protection drops below a health threshold, switch to heal priority until stabilized.
- If target is out of line of sight or too far away, move toward a valid casting position or fall back to melee if configured.
- Self buffs are used out of combat at long intervals, not spammed mid fight.

Spell choices and cooldowns are data driven so that new NPC types can share the same logic.

### 18.4 Boss and Named Behavior Model

Boss and mini boss NPCs are built from the same archetypes and flags, with additional phase definitions. Phase logic is intentionally simple and data driven to keep scripting light.

Phase triggers:

- Health thresholds, for example 75, 50, 25 percent
- Timers since combat start
- Number of adds alive or dead

Phase actions:

- Spawn adds at specific locations or near random players
- Change active spell list or add new abilities
- Adjust aggro behavior, for example more frequent taunt style abilities
- Apply self buffs like rage, increased damage, or increased resistances
- Trigger simple emotes or zone wide text messages

Example implementation pattern:

- Phase 1: Baseline behavior using starting archetype and spell list.
- Phase 2 at 75 percent: Spawn two adds and start using area damage spell list.
- Phase 3 at 50 percent: Increase melee output, become immune to mez or charm.
- Phase 4 at 25 percent or timer expiry: Enrage mode, faster attack rate and higher damage.

All of this is configured in NPC data and optional boss script records rather than hard coded per encounter.

### 18.5 Pathing, Leashing, and Camp Friendly Behavior

To support classic camp and pull tactics, the AI respects clear rules:

- Patrol paths use predefined waypoints or randomized wander around a center point.
- Body pulls occur at the edge of AggroRadius; social adds are determined by SocialRadius and IsSocial flag.
- When leash conditions are met, NPCs break combat, return to spawn, and reset health and hate.
- Doors, elevators, and one way transitions can be marked as path blockers to prevent NPCs from chasing into invalid areas.

These behaviors ensure that groups can split pulls, control adds, and maintain safe camps in a way that feels very close to original EverQuest.

---

### 19. NPC Archetypes & Roles

Archetype-based design allows rapid content creation while ensuring enemy variety and tactical diversity.

Each NPC type is authored from a shared template + behavior package.

### 19.1 Standard Archetypes

**Melee Bruiser**  
High HP/AC, steady damage, low ability variety; small chance for special melee moves.

**Skirmisher**  
Lower HP, higher burst; uses positional and fleeing tactics.

**Caster**  
Prioritizes spells (nukes, DoTs, debuffs, CC); attempts to maintain range.

**Priest / Healer**  
Heals allies below thresholds; uses defensive buffs, cleanses, panic CC.

**Ranged**  
Uses bows/throws; kites if threatened.

**Summoner**  
Has a bound pet or can call adds; pet has independent hate.

**Controller**  
Uses mez/root/fear/slow equivalents; high hate generation, fragile body.

**Boss / Elite**  
Layered mechanics tuned for group/raid coordination.

### 19.2 Template Components (Server Authoritative)

Shared components:

- **StatBlock:** Level, HP, AC, resists, attack tables.
- **FactionPackage:** Who they assist/hate, faction hits.
- **LootPackage:** Per-NPC or per-archetype loot.
- **AbilityPackage:** Melee specials and spells.
- **NavigationPackage:** Roaming, leashing, pathing.
- **BehaviorPackage:** State machine rules.

---

## 20. Perception & Aggro Model

This section defines how NPCs detect and respond to players, supporting tactical pulling and encounter management.

### 20.1 Aggro Types

**Proximity Aggro**  
NPC acquires a target within radius + line of sight.

**Social Aggro (Assist)**  
NPCs of same social group assist within assist radius.  
"Call for help" behavior for guards, pack beasts, etc.

**Damage Aggro**  
Being hit always adds hate to attacker.

**Spell Aggro**  
Debuffs and CC create extra hate by design; heals generate split hate.

### 20.2 Pulling Support

REQ supports classic pulling:

- Body pulls (edge of radius).
- Line-of-sight pulls.
- Single-pull tools (pacify equivalents, mezzing, etc.).
- Feign-drop / stealth-drop mechanics for design space.

---

## 21. Hate (Threat) System — NPC Combat Brain

The NPC threat system determines targeting priority and creates the tactical space for tanks, DPS, and healers to play their roles.

Each hostile NPC tracks `HateTable[EntityID] = float`.

Top hate entity is current target unless overridden.

**Hate sources:**

- **Melee:** `hate += damage × meleeScalar`
- **Spells:** `hate += damage × spellScalar`
- **Healing allies:** `hate += healAmount × healScalar` distributed across attackers
- **Crowd control:** Flat bonus hate
- **Snap hate skills:** Large instant hate with cooldown

**Threat decay / memory mechanics:**

- Some NPCs can "memblur" or reset hate lists.
- Threat decay slowly forgets non-damaging enemies over time.

---

## 22. Core AI State Machine

NPC behavior is driven by a simple, readable state machine that handles idle, combat, and recovery behaviors.

**States:**

**Idle** – Patrol/roam logic, low-frequency awareness checks.

**Alert** – Target detected; quick faction check to decide hostility.

**Engaged** – Hate list active; combat loop running.

**Leashing / Reset** – If beyond allowed range or no valid targets, clear hate and return to spawn.

**Fleeing** – Triggered by morale/HP thresholds or archetype rules.

**Dead** – Corpse container spawns; loot enabled; faction/XP events emitted.

---

## 23. Movement & Navigation Behaviors

Movement patterns create world believability and tactical encounter variety.

### 23.1 Patrol Types

- Static Guard
- Waypoint Patrol
- Random Roam within home radius
- Hunter Roam targeting specific prey classes/factions

### 23.2 Leash & Home Radius

**Outdoor mobs:** Soft leash with extended chase and delayed reset.

**Dungeon mobs:** Hard leash to prevent dragging.

### 23.3 Pathing Variance

Mobs do not path perfectly; light variance produces "ambush angles" and imperfect pulls.

---

## 24. Combat Behavior Packages

Behavior packages define how different NPC archetypes act during combat, creating diverse tactical challenges.

### 24.1 Melee Behavior

Maintain melee range.

Use specials based on cooldowns and target status.

Optional positional preferences (front/back).

### 24.2 Caster Behavior

Maintain preferred distance band.

**Priority stack:**

1. Survival (shield, blink, self-heal)
2. Control (mez/root/fear/slow)
3. Debuffs (resist-down, armor-down)
4. DoTs
5. Nukes

**Under melee pressure:**

- Step back / blink / root-and-run.
- Use panic CC if available.

### 24.3 Healer Behavior

Watch ally HP thresholds.

Choose heal sizes by urgency (efficient vs panic).

Chain-heal bosses if not interrupted (creates CC/interrupt checks).

### 24.4 Ranged / Kiter Behavior

Maintain ranged distance.

Retreat if the target closes.

Use snare/slow to support kiting.

### 24.5 Pet / Add Behavior

Adds inherit master's hate bias.

Pets auto-defend their master.

Leash behavior tied to master unless scripted otherwise.

---

## 25. "Classic MMO" NPC Traits

These special abilities and behaviors create memorable, dangerous encounters that require group coordination and role execution.

Config toggles in AbilityPackages:

**Flee at Low Health** – Mobs flee at ~15–25% HP, seeking allies.

**Enrage** – At ~10–15% HP, gain haste/crit.

**Flurry** – Chance for multiple extra swings.

**Rampage** – Periodic attacks hitting top hate + nearby targets.

**Knockback** – On hit or timed; breaks formation, interrupts casting.

**Dispel / Strip Buff** – Remove beneficial effects from players.

**Summon (Anti-Kite)** – Pulls distant targets back to mob.

**Charm / Fear / Mez** – Control spells for certain controllers/bosses.

**Lifetap / Drain** – Damage heals the mob; requires DPS or interrupts.

**Pack Instinct / Social Chains** – Aggressive assists; bad pulls snowball.

---

## 26. Boss & Elite Encounter Design

Boss fights are scripted encounters with readable mechanics and clear counters. Quality over quantity: fewer mechanics, better execution.

Bosses require group coordination and are authored with explicit mechanic tracks.

### 26.1 Boss Design Rules

- 1–3 signature mechanics per boss (not 8).
- Mechanics must be readable (telegraphs, audio cues, timers).
- Every mechanic has at least one counter.

### 26.2 Boss Mechanic Library

Reusable building blocks:

- Phase changes (HP thresholds, new behavior)
- Add waves (healer / controller / bruiser adds)
- Area effects (PBAE, targeted AE, persistent DoT zones)
- Dispellable buff windows (shields/haste that must be stripped)
- Cure / cleanse checks (stacking debuffs)
- Interrupt checks (channeled big abilities)
- Teleport / banish (tank displacement / player reposition)
- Gaze / directional mechanics (face/not-face rules)
- Soft enrage timer (ramping damage)
- Loot / objective locks (must kill lieutenants, disable pylons, etc.)

### 26.3 Group vs Raid Boss Tuning

**Group Bosses:**  
Expect 1 tank, 1 healer, 1 CC/support, 2–3 DPS.  
Mechanics are forgiving but punishing if ignored.

**Raid Bosses:**  
Multi-tank requirements, heavy add control, positional objectives.  
Likely later milestone content.

---

## 27. Spawn System & Population Rules

The spawn system populates zones with enemies, controls respawn cadence, and creates camping incentives.

### 27.1 Spawn Tables

Each zone has:

- Spawn points
- Mob archetype list
- Weighted chances
- Respawn time ± variance
- Roam radius / waypoint routes
- Named/elite chance

### 27.2 Placeholder / Named Logic

Certain camps roll a "named" mob from a pool.

Spawn variance encourages camping.

### 27.3 Day/Night & Weather Variants (Optional)

Nocturnal mobs at night.

Weather-based spawns (fog beasts, storm elementals, etc.).

---

## 28. Data Architecture & Data-Driven Authoring

REQ uses **structured JSON files as the canonical source of truth** for all shared gameplay data. Servers and client both read from exports generated from these JSON definitions. Unreal DataAssets are used only for **visual/client-only details** (meshes, animations, VFX, icons).

## 28.1 Canonical Game Data (JSON)

Core systems are defined in version-controlled JSON (or similar structured formats), for example:

- `items.json` – Items, equipment, consumables.
- `spells.json` – Spells and abilities (cast times, costs, effects).
- `classes.json` / `races.json` – Class/race definitions and restrictions.
- `factions.json` – Faction IDs, names, and relationships.
- `npcs.json` – NPC templates (stats, behavior, visuals, loot table IDs).
- `loot_tables.json` – Loot table definitions and weighted entries.
- `spawns_<zone>.json` – Spawn points, probabilities, and schedules per zone.
- `quests.json` (later) – Quest structures, requirements, and rewards.

Each record is structured with DB-friendly fields, for example (conceptually):

- `id` (numeric or GUID)  
- `name` (string)  
- `archetype` / `class` / `race`  
- `flags` (bitfields or tag arrays)  
- `stat_mods { hp, mana, str, sta, ... }`  
- `faction_id`, `loot_table_id`, `spawn_group_id`  
- `visual_id` (reference into client-side visual asset data)  

This design makes it straightforward to later migrate JSON into a relational database without redesigning the schema.

## 28.2 Client vs Server Data Responsibilities

- **Servers (C++ / Boost.Asio):**
  - Load canonical JSON at startup and cache in memory.
  - Use game data for:
    - Stat calculations, damage formulas, resist checks.
    - Faction hits and con tiers.
    - Spawn tables and AI behavior flags.
    - Loot rolls and quest progression.
  - Never rely on Unreal-specific assets or paths.

- **Client (UE5.7):**
  - Receives only IDs and compact data from the server (item IDs, spell IDs, NPC template IDs, faction IDs).
  - Uses those IDs to look up **visual and UI data**, which can be:
    - JSON mirrored into UE, or
    - Generated into **DataTables/DataAssets** during the content build step.
  - Visual DataAssets are **not** the source of gameplay truth; they reference the canonical IDs coming from JSON.

Examples of client-only content:
- Mesh/animation blueprint for NPC visual ID.
- Icon atlases for item/spell IDs.
- Sound/VFX sets for ability IDs.

### 28.3 REQ Data Editor (Authoring Front-End)

To make data editing accessible and safe:

- A future **REQ Data Editor** (desktop or web app) will sit on top of the JSON files and provide:
  - Forms for items, spells, NPCs, loot tables, factions, and spawns.
  - Drop-downs for referencing existing IDs (class, race, faction, loot table).
  - Validation (required fields, ID uniqueness, value ranges).
  - Export commands:
    - **Export for Server** – Writes/validates canonical JSON for runtime.
    - **Export for Client** – Writes JSON/CSV/DataTables used by UE.

Initially, designers can work directly with JSON and spreadsheets; the editor formalizes this workflow over time.

### 28.4 Adding a New Mob (Example Flow)

Content designers can create new NPCs without programmer intervention by updating JSON definitions and, optionally, Unreal visual assets.

To add a new mob:

1. **Define NPC Template (JSON)**  
   In `npcs.json`, add a new entry:

   - `npc_id` (unique ID)
   - `name`
   - `level_range`
   - `archetype` (Warrior, Priest, Caster, etc.)
   - `stat_block` (HP, AC, attack, resists)
   - `faction_id`
   - `loot_table_id`
   - `behavior_flags` (social, caster, flees_at_low_hp, etc.)
   - `visual_id` (link to client-side visuals)

2. **Define Behavior Package (JSON)**  
   In `behaviors.json` (or as part of `npcs.json`):

   - AI archetype (melee bruiser, healer, nuker, crowd-control, etc.)
   - Preferred abilities and their usage conditions.
   - Aggro radius, assist radius, leash distance, roam patterns.

3. **Define Ability Package (JSON)**  
   In `spells.json` / `npc_abilities.json`:

   - Lists of spell/ability IDs the NPC can use.
   - Cooldown ranges, cast priorities, HP thresholds.
   - Special scripts/triggers (on-aggro, on-HP-percent, on-timer).

4. **Define Faction Package (JSON)**  
   In `factions.json`:

   - Ensure the NPC’s `faction_id` exists.
   - Configure:
     - Starting standing by race/class/deity if needed.
     - Faction hits for kills or specific quest actions.
     - Ally/assist lists for other NPC groups.

5. **Define Spawn Entry (JSON)**  
   In `spawns_<zone>.json`:

   - Spawn point ID and location (position + heading).
   - Reference to `npc_id` or spawn group that includes this NPC.
   - Respawn time and variance.
   - Day/night or weather conditions, if used.

6. **Hook Up Client Visuals (UE)**  
   - Create or assign a **visual DataAsset / DataTable row** for the `visual_id`:
     - Skeletal mesh and animation blueprint.
     - Materials, tint, size scale.
     - Idle/combat/death animations.
   - Ensure the client-side lookup for `visual_id` → asset works.

Once these steps are complete and exports are run:

- **Server side:**  
  - Zone servers will spawn the new mob based on `spawns_<zone>.json` and use the `npc_id`’s stats, abilities, and behaviors.
- **Client side:**  
  - When the server tells the client “NPC with `npc_id` X appears at position Y,” the client resolves `visual_id` to the correct model and animations and displays it.

**No code changes are required for typical mobs**; only JSON data and visual asset updates are needed.

### 28.5 Account & Character Persistence Model

Character data lives in a **global account store**, not bound to any single World server process. This enables multi-world setups and clean character transfers.

#### 28.5.1 Logical Layout

- **Account** (top-level entity):
  - `account_id` (primary key)
  - Login credentials / auth data
  - Security flags (bans, locks, 2FA status, etc.)
- **Character** (child of account):
  - `character_id`
  - `name`, `race`, `class`, `deity`
  - `level`, XP, skill values
  - `home_world_id` (canonical home)
  - `last_world_id`, `last_zone_id`, `last_position`
  - Inventory, bank, keyring, quest flags
  - Faction standings, bind point, resurrection/bind history

Early implementation can be **per-account JSON blobs**:

text
/data/accounts/<account_id>.json

with a clear schema that maps 1:1 to future relational tables.

#### 28.5.2 World & Zone Responsibilities

Login server

Talks to the global account/character store.

Hands out session tokens and the list of worlds for that account.

World server

Queries the store for characters where account_id == X and home_world_id == this_world.

Applies world-specific rulesets or flags (e.g., seasonal, hardcore).

Zone server

Receives a character payload from World on successful handoff.

Persists snapshots back via the World (or directly) at safe points:

Periodic autosaves.

Zone transitions.

Camping/logging out.

No single world “owns” the canonical character record; all worlds are consumers of the shared store.

#### 28.5.3 Transfers & Migrations

Because characters are stored globally:

A transfer tool can:

Change home_world_id for a character.

Duplicate a character into a new world with a new ID (for seasonal resets or progress copies).

World-specific flags (e.g., season_id, is_legacy_copy) can be added without changing the core layout.

Backups/snapshots can be made at the account or character level for rollback or restore.

Design goal: make cross-world and cross-server migrations a data operation, not a code rewrite.

#### 28.6 World Rulesets and Hot Zones (world_rules.json)

REQ separates low-level server topology from high-level gameplay rules. World configuration focuses on networking and process management, while world rulesets define pacing, difficulty, and quality-of-life behavior.

World configuration versus rules

- world_config.json
    - World identifier and name
    - Listening addresses and ports
    - Zone membership and topology
    - Auto-launch settings for zone servers
    - Logging and metrics destinations
- world_rules.json
    - Experience and loot pacing
    - Death and corpse behavior
    - PvP behavior
    - Quality-of-life settings
    - Hot-zone definitions

Each world_config entry references a ruleset identifier or a specific world_rules file, allowing multiple worlds to share a ruleset while differing in name, population, or zone layout.

Ruleset contents

A world ruleset can include, at minimum:

- Experience and loot:
    - Base XP rate
    - Group and raid bonuses
    - Loot and coin drop multipliers
    - Rare drop and named NPC multipliers
- Death and corpse rules:
    - XP loss on death and its scaling
    - Whether corpse runs are required
    - Corpse decay timers and retrieval behavior
    - Special rules for safe zones or training dummies
- PvP behavior:
    - PvP enabled or disabled
    - Mode: duels only, teams, free-for-all, or zone-limited PvP
    - Penalties and rewards for PvP kills
- Quality-of-life options:
    - Availability of minimap and zone maps
    - Visibility of con colors and faction-based outlines or pulses
    - Presence of quest trackers and objective counters
    - Use of corpse arrows, navigation helpers, and similar guidance
- Hot zones:
    - A list of zone entries with:
    - A zone identifier
    - XP and loot multipliers specific to that zone
    - Optional start and end dates for scheduled hot zones

Integration with world selection

The world list shown to players includes a concise description of the active ruleset and prominent tags or icons representing key characteristics, such as:

- Classic, Classic with QoL, Casual XP, or Hardcore
- PvP-enabled or PvE-only
- Presence of strong UI helpers versus minimal guidance

By storing these settings in world_rules.json and keeping them separate from network configuration, REQ makes it straightforward for operators to run multiple worlds with distinct identities while reusing the same core binaries and data definitions.

### 28.6.1 Classic REQ Baseline Ruleset

The Classic REQ ruleset aims to match late classic or early Luclin era behavior as closely as practical, while still allowing tuning by data.

Identity

- Ruleset tag: classic_req
- Focus: slow leveling, meaningful death, minimal quality of life helpers, strong reliance on groups.

Experience and loot pacing

- Base experience rate: 1.0, defined as parity with reference EverQuest era values.
- Group bonus: increasing bonus per extra group member, with diminishing returns, so that full groups level noticeably faster than solo play.
- Raid experience: minimal bonus, since raid content is focused on loot.
- Loot and coin: tuned so that vendor trash and common items cover spells, food, and basic gear upgrades without overwhelming the economy.

Death and corpse behavior

- Experience loss on death begins at level 6, as in classic EverQuest.
- Loss amount scales by level and can cause loss of a level if enough deaths occur.
- Resurrections restore a percentage of the lost experience based on spell or service quality.
- On death, a corpse is created with most or all equipped items and coin, depending on ruleset flags.
- Players revive at bind point locations and must recover their corpse unless a resurrection is used.

These behaviors are data driven so that community operators can very closely approximate original classic values.

Quality of life defaults

- No minimap or quest tracker by default.
- No automatic path indicators or sparkles for objectives.
- No automated group finder beyond who lists and manual chat channels.
- Chat, social tools, and guild support are present, but discovery of locations and mechanics is left to players.

PVP behavior

- Classic REQ baseline assumes a PVE oriented world.
- Duels can be enabled as mutual consent encounters with no experience loss.
- Separate PVP oriented rulesets can change this, but are not part of the baseline.

### 28.6.2 Example Ruleset Presets

Using the same world_rules.json structure, operators can define presets such as:

- classic_req
    - Baseline described above.
    - Closest to original EverQuest intent.
- classic_req_qol
    - Same experience and death settings.
    - Adds minimap, basic quest progress counts, and optional corpse location hints.
- progression_req
    - Slightly faster experience and reduced death penalties.
    - Intended for players who want to see more content in less time.
- hardcore_req
    - Slower experience, harsher death penalties, limited or no resurrection options.

These are configuration examples only; the underlying mechanics are the same.

### 28.7 Quest Execution and Scripting Model

REQ’s quest system is primarily data-driven, with optional script hooks reserved for complex or highly cinematic content. The goal is to support large numbers of simple, classic-style quests while also allowing richer multi-step narratives that are easier to follow than many original EverQuest quests.

Data-driven quests

Quests are defined as structured data in quests.json. Each quest includes:

- Metadata:
    - Quest identifier
    - Name and in-game descriptions
    - Recommended level range
    - Suggested group size and faction themes
- State model:
    - Logical states such as NotStarted, Active, one or more intermediate states, Completed, and Failed
- Steps:
    - A sequence or graph of steps that the player must complete
    - Clear prerequisites and completion conditions for each step
    - Associated narrative text and journal entries

Common step types are standardized so most quests can be authored without scripts, for example:

- Kill: defeat a number of NPCs by template ID, tag, or location
- Collect: obtain specific items from drops, containers, tradeskills, or merchants
- Talk: speak to specific NPCs, optionally in a particular order
- Explore: reach a location, area, or volume in a zone
- Interact: use an object, device, trigger, or world item
- ReachThreshold: achieve a faction, skill, or stat threshold
- UseItem: use a particular item on self, a target, or a world object

Each step defines:
- Which events it listens to (kill, loot, talk, interact, enter region, use item)
- Required counts or thresholds and any time limits
- Prerequisites such as level, class, race, faction, or completion of other quests
- On-complete actions such as awarding XP, currency, items, spells, faction changes, or setting flags that unlock other content

Server-side execution

The World and Zone servers host the authoritative quest state machines:

- Maintain per-character quest state and progression
- Subscribe to gameplay events from combat, loot, dialogue, exploration, and interaction
- Evaluate step conditions and advance quest states when requirements are met
- Emit compact quest updates to the client, including journal entries and progression markers where allowed by the ruleset

Quest state is stored alongside other character data in the global account and character persistence model, allowing quests to span multiple sessions, zones, and even worlds where appropriate.

Script hooks for special content

For rare or particularly complex quests, steps may reference an optional script identifier:

- Scripts are used for situations that cannot be easily expressed with standard step types, such as:
    - Multi-NPC choreography and timed sequences
    - Puzzles with custom logic
    - Zone-wide or world events affecting many players at once
- Scripts run on the server and orchestrate events, but still rely on the core systems for combat, movement, and rewards. They cannot bypass central validation of damage, healing, or item granting.

Quest UX and ruleset interaction

Presentation of quest information to the player is controlled in part by the world ruleset:

- Classic-style worlds may emphasize text and journal-only guidance with minimal markers
- Modern-style worlds may enable optional progress trackers, objective counters, or gentle map hints
- All of these presentations are views on the same underlying quest data model

This approach keeps quest authoring accessible to designers and GMs, while still allowing standout questlines and world events where the extra expressiveness of a scripting hook is warranted.

### 28.8 Worked Example: Classic Style Newbie Camp

This example shows how a simple outdoor camp can be defined in data in a way that mirrors a classic EverQuest starter area.

Context

- Zone: generic human starter field, level range 1 to 6.
- Camp: small orc or bandit camp near a road and guard tower.

Spawn setup

In a zone specific spawn file:

- Spawn group id: starter_orc_camp.
- Location: small radius around a campfire and tents.
- Entries:
    - Orc Grunt or Bandit Thug (melee)
    - Orc Scout or Bandit Scout (ranged)
    - Very low chance placeholder for a named officer.
- Respawn timer: about six minutes with slight variance.
- Social aggro: members of the same camp assist within a short radius, and may call nearby pathers of the same type.

Named placeholder example

- Named npc id: camp_officer_named.
- Level: a bit higher than the regular camp mobs.
- Abilities: slightly higher damage and health, maybe one minor buff.
- Spawn: low chance in place of a scout entry in the camp group.

Loot tables

- Common table for regular orcs or bandits:
    - Rusty weapons, cloth or leather scraps, small coin.
- Named table:
    - Same common items.
    - One or two simple low level items such as a belt, ring, or weapon with small stat bonuses.

Tables are defined in loot_tables.json and referenced from the npc templates.

Quest tie in

Simple quest chain in a zone or city quest file:

- Step 1: Guard or trainer asks the player to kill a small number of orcs or bandits and return proof.
- Step 2: Follow up quest asks for the head, token, or badge of the camp officer.
- Rewards: experience, faction with the local guard or city, and a basic piece of gear.

This structure is intentionally generic so that it can mirror whatever original EverQuest zone you import data from, while using the same data model.

## 29. Debug & Telemetry (Dev Tools)

Development and tuning require visibility into AI and combat systems. These tools are dev-only and essential for iteration.

Dev-only tools to debug AI and combat:

- Hate table viewer (top N entries).
- AI state overlay.
- Spawn point visualizer.
- Ability timer traces.
- Leash radius gizmos.
- Boss mechanic timeline logs.

---

## 30. UMG / UI Package Spec (Widgets to Create)

This section is the complete blueprint for all UI widgets in the game, organized by context and function.

### 30.1 UI Architecture Notes

Use UUserWidget subclasses for layout; C++ base classes for logic.

Prefer events/dispatchers over Blueprint property bindings.

MVVM or ViewModels can be added later.

CommonUI is optional for stacked screen flows.

### 30.2 Front-End Flow Widgets

**WBP_Login**  
Username/password fields; Login, Register, Quit; error text.

**WBP_WorldSelect**  
Scroll list of worlds; latency/ping; Enter World / Back.

**WBP_CharacterSelect**  
Character list (name/class/level/zone); Play, Create, Delete, Back.

**WBP_CharacterCreate**  
Race select panel; class select panel (filtered by race); cosmetic sliders; name entry; Create / Back.

**WBP_LoadingScreen**  
Zone name, tips, progress bar.

### 30.3 In-Game HUD Widgets

**WBP_HUDRoot (top-level container)**

**Top-left:** WBP_PlayerFrame, WBP_TargetFrame, WBP_GroupFrame

**Bottom-center:** WBP_HotbarPrimary, WBP_CastBar

**Bottom-left:** WBP_ChatWindow

**Top-right:** WBP_BuffBar_Player, WBP_BuffBar_Target

**Minimap overlay:** WBP_Minimap (optional)

**Sub-widgets:**

- **WBP_PlayerFrame** – Name, level, HP/mana/endurance bars, numeric values, regen indicators.
- **WBP_TargetFrame** – Target name/level/type, HP bar, con-color indicator, buffs/debuffs row.
- **WBP_GroupFrame** – List of WBP_GroupMemberRow (up to 6).
- **WBP_HotbarPrimary** – 1–3 hotbars with WBP_HotbarSlot entries; drag/drop, cooldown overlay, keybind label.
- **WBP_CastBar** – Spell/ability name, progress bar, interrupt flash.
- **WBP_BuffBar_Player/Target** – Horizontal wraps of WBP_BuffIcon.
- **WBP_ChatWindow** – Tabs (WBP_ChatTab), scrollbox, input line, channel selector.

### 30.4 Gameplay Menus / Windows

**WBP_InventoryWindow** – Bag grid (WBP_InventorySlot), weight/coin, Sort/Split/Close.

**WBP_EquipmentPaperdoll** – Equipment slots, optional 3D preview, derived stats (WBP_StatRow).

**WBP_ItemTooltip** – Icon, name, rarity color, stats, requirements, flavor text, comparison.

**WBP_LootWindow** – Loot and buttons: Loot / Need / Greed / Pass / Loot All.

**WBP_MerchantWindow** – Vendor list, player sell list, search and filters.

**WBP_TradeWindow** – Two-column trade grid, currency offers, Ready/Confirm states.

**WBP_SpellbookAbilities** – Tabs by school/category, list (WBP_SpellIconRow), drag to hotbar.

**WBP_QuestJournal** – Quest list (WBP_QuestRow) + detail pane with tracking toggles.

**WBP_MapWindow** – Zone map, player marker, group markers, optional notes.

**WBP_RespawnWindow** – Death summary; Respawn / Wait for rez / Bind location options.

**WBP_SystemMenu** – Resume / Options / Keybinds / Logout / Quit.

**WBP_Options** – Video / Audio / Gameplay / UI scale with Apply/Cancel.

**WBP_Keybinds** – Action list + remap buttons (Enhanced Input).

**WBP_DebugOverlay (dev-only)** – Net stats, server tick, position, target ID, etc.

### 30.5 Reusable Sub-Widgets

These small, reusable components ensure UI consistency and reduce duplication.

- WBP_InventorySlot
- WBP_HotbarSlot
- WBP_GroupMemberRow
- WBP_ChatTab
- WBP_BuffIcon
- WBP_QuestRow
- WBP_SpellIconRow
- WBP_StatRow

These ensure consistency and keep UMG manageable.

---

## 31. Matching C++ Base Classes

C++ base classes provide the logic layer while Blueprint widgets handle layout, styling, and animation.

Create C++ base classes so Blueprints handle layout/animation while C++ manages logic.

### 31.1 Core

**UAntUserWidgetBase : UUserWidget**

Helpers: `Show()`, `Hide()`, `SetInputModeGame()`, `SetInputModeUI()`.

Optional ViewModel pointer for MVVM later.

### 31.2 Front-End

- ULoginWidgetBase
- UWorldSelectWidgetBase
- UCharacterSelectWidgetBase
- UCharacterCreateWidgetBase
- ULoadingScreenWidgetBase

### 31.3 HUD

- UHUDRootWidgetBase
- UPlayerFrameWidgetBase
- UTargetFrameWidgetBase
- UGroupFrameWidgetBase
- UHotbarWidgetBase
- UCastBarWidgetBase
- UBuffBarWidgetBase
- UChatWindowWidgetBase

### 31.4 Windows

- UInventoryWidgetBase
- UEquipmentWidgetBase
- UItemTooltipWidgetBase
- ULootWidgetBase
- UMerchantWidgetBase
- UTradeWidgetBase
- USpellbookWidgetBase
- UQuestJournalWidgetBase
- UMapWidgetBase
- URespawnWidgetBase
- USystemMenuWidgetBase
- UOptionsWidgetBase
- UKeybindsWidgetBase

### 31.5 BindWidget Usage

Use `meta = (BindWidgetOptional)` where layout may vary to avoid hard crashes.

Avoid Blueprint "property binding" getters for rapidly changing values; drive updates via events/dispatchers.

---

## 32. Next Steps

The immediate path forward: prioritize one vertical slice and implement it end-to-end.

1. Create the widget Blueprints listed above as empty shells in UE.

2. Choose a first "packet" to implement end-to-end (e.g., Frontend flow, Core HUD, or Inventory/Loot/Tooltip).

3. For that packet, define detailed C++ headers, function signatures, and event flows so they can be implemented and compiled cleanly.

### 32.1 Vertical Slice Target: Classic Starter Flow

For an EverQuest emulator style project, the first vertical slice should exercise the full login to level 5 loop rather than showcase a unique custom region.

Target features for the slice:

- Account login, world list, and character select.
- Character creation for at least one race and a few classes, with correct starting city and bind point.
- Load into a starter city or safe outpost and then into the adjacent newbie field.
- Basic combat against simple outdoor enemies using the prototype class kits defined in section 17.
- Experience gain, level up, skill ups, basic coin and loot.
- Death, corpse creation, bind point respawn, and at least one method to recover the corpse.
- One or two very simple EverQuest style quests tied to the nearby camp.

No specific zone names are fixed in this document. The intent is that the same systems can back either original EverQuest data (for personal learning) or original REQ content in future.

---

## 33. Deployment & Emulator Distribution

REQ is intended to function both as:

1. A **flagship official server** operated by the project owner.
2. A **distributable emulator stack** that others can host, configure, and extend.

### 33.1 Stack Components

A typical deployment consists of:

- **login_server**
  - Handles authentication, account creation, and session tokens.
- **world_server**
  - Hosts the world list and character selection.
  - Routes players to zones.
  - Optionally auto-manages Zone server processes (see 14.6).
- **zone_server**
  - One or more processes, each simulating a specific zone or shard of a zone.
- **chat_server** (optional)
  - Global/zone/guild/party channels, tells.
- **tools**
  - Data editor, zone exporter, loot/quest editors, admin console.

All servers are 64-bit C++ processes depending only on Boost.Asio and standard libraries, runnable on **Windows or Linux**.

### 33.2 Configuration-Driven Install

Hosts configure the stack using JSON files:

- `login_config.json`
  - Listen address/port, database/account store configuration, registration rules.
- `world_config.json`
  - World name, description, ruleset tag (e.g., *Classic*, *Progression*, *PvP*).
  - `auto_launch_zones` toggle and `zones[]` definitions.
- `zones_config/`
  - Per-zone overrides, spawn density multipliers, XP modifiers, etc.

Goal: a new host should be able to:

1. Download binaries + sample configs.
2. Adjust a small set of JSON files.
3. Start `login_server`, `world_server`, and either:
   - Run `zone_server` manually, or
   - Let the world auto-launch zones if enabled.

### 33.3 Flagship vs Community Servers

- **Flagship server (official)**
  - Curated ruleset and content cadence.
  - Used as the reference implementation for new features.
- **Community servers**
  - Configure their own:
    - Ruleset/QoL presets.
    - XP rates, loot multipliers.
    - Allowed client versions and mods.
  - Can opt into or out of future REQ features as they see fit.

To support this, protocol and data formats are:

- **Versioned**, with clear upgrade paths.
- Documented for community operators (minimal “tribal knowledge”).

### 33.4 Packaging

Longer term, official distributions may include:

- **Bare executables + configs** (zip/tarball) for advanced operators.
- Optional **container images** (Docker) for easier orchestration.
- Sample **systemd/service** templates for Linux and service configs for Windows.

Design goal: running a basic REQ stack should be as simple as running a dedicated game server, not a full dev environment.

### 33.5 Patch Server and Launcher Architecture

REQ uses a manifest-based patching system so that both the flagship server and community-hosted servers can distribute updated clients and data safely.

Patch manifest

Client updates are described by a patch manifest, typically named patch_manifest.json or similar. The manifest is hosted on a standard HTTP or HTTPS endpoint, such as a simple web server or content delivery network. It includes:

- A client version identifier and optional channel label such as live, test, or custom
- A list of files or bundles with:
    - Relative paths from the install root
    - File sizes
    - Cryptographic hashes for integrity verification
- Optional grouping of files into categories such as core, high-resolution assets, or localization packs

REQ launcher

A small launcher application is responsible for keeping a client installation in sync with the selected server’s patch manifest. On startup, the launcher:

1. Reads a configuration that specifies the patch server URL and channel to use
2. Downloads the latest patch manifest from that endpoint
3. Compares the manifest to the local installation to determine missing or outdated files
4. Downloads and verifies only the changed files using the hashes in the manifest
5. Applies updates and then starts the Unreal Engine client with appropriate parameters (for example, pointing it at a chosen login server or world)

The launcher can support multiple server profiles so players can switch between the flagship server and community-run servers that maintain their own patch URLs and content.

Community-hosted stacks

Community operators:

- Deploy backend binaries (login, world, zone, and optional chat) along with their configuration files
- Host a web-accessible patch directory containing their patch manifest and any client-side assets or data bundles required by their ruleset and content choices

As long as patch manifests and protocol versions remain compatible, the same launcher can be reused across different deployments by switching profiles or configuration files.

Design goals

- Keep patching simple to host and operate using standard web infrastructure
- Make it easy for players to stay up to date with minimal manual intervention
- Allow community administrators to maintain their own cadence of client and content updates without requiring changes to the core emulator codebase

### 33.6 Tooling Scope by Milestone

Because REQ aims to be an EverQuest emulator replacement, tooling needs to support both native REQ content and potential import or mapping from existing EverQuest style data.

Vertical slice tooling

- Author configuration and content directly in JSON files and Unreal data assets.
- Provide small validation scripts to check for broken ids, missing fields, and bad references.
- Use Unreal Editor only for world layout and placement, similar to how classic zones are defined by external tools.
- Provide simple in game commands for teleport, spawn toggles, level setting, and corpse handling for testing.

Post vertical slice tooling

After the core loop is working:

- Build a neutral data editor that can view and edit NPCs, loot tables, quests, factions, and spells in the same schema that the servers use.
- Optionally provide import or mapping paths that can translate external EverQuest style data sets into REQ json records where license and personal use allow.
- Develop dashboards for monitoring world, zone, and player counts, plus basic logs for debugging.

Tooling should never bake in specific lore names or IP. It should only care about ids and fields. This keeps REQ usable as a clean emulator style platform where operators can decide which content to load.

### 34. Quest and Event System Overview

REQ uses a lightweight server side scripting layer with data driven definitions for quests and zone events. The goal is to match classic EverQuest style quest flows while remaining easy to extend and safe to modify.

### 34.1 Goals and Design Direction

- Support common EverQuest quest patterns such as item hand ins, kill counts, dialogue branches, and faction gates.
- Keep core logic inside the server, with scripts that can be edited without recompiling.
- Avoid complex visual editors at this stage, but keep data structures clean enough for tooling later.
- Make it straightforward to port logic from existing emulator quest scripts if desired.

REQ assumes a small embedded scripting runtime on the zone server, for example Lua, with a narrow API surface. Quests are defined in data files and have optional script files for more involved behavior.

### 34.2 Quest Data Model

Each quest entry in quests.json contains:

- QuestId (numeric primary key)
- Name and short description
- Recommended level range
- Starting NPC and starting zone identifiers
- Required classes, races, or deity alignments if any
- Repeatable flag and lockout rules
- State machine definition with allowed states
- Objective list
- Rewards block
- Faction changes block

Quest states are simple and explicit:

- Unseen
- Offered
- InProgress
- Completed
- Failed

The server tracks current state per character and notifies the client when it changes, so the client can update journal UI.

### 34.3 Objective Types

An objective is a unit of progress inside a quest. The following types are supported in the base system:

- Kill: defeat N targets that match a given NPC template or tag.
- Collect: obtain N items, either from drops, ground spawns, or vendors.
- Talk: speak with a specific NPC and select a required dialogue option.
- Explore: reach a location or zone sub area.
- Escort: keep a designated NPC alive while it moves along a path.
- UseItem: use a quest item on a target NPC or location.
- ReachFaction: reach a required faction tier with a specific faction id.
- LevelUp: reach a specific character level.
- Timed: complete another objective within a time limit.

Objectives can be combined into steps:

- Sequential steps, where Step 2 only unlocks once Step 1 is complete.
- Parallel objectives, where several items must be collected before turning in.

### 34.4 Triggers and Hooks

Zone and world servers generate events that quest scripts and the generic quest engine can listen to. Key events include:

- Player kill events (player, target, zone, quest tags)
- Item looted or obtained
- Item handed to an NPC
- Player enters zone or region
- Dialogue line selected in NPC interaction
- Faction tier change
- Level up
- Timer ticks for quests with time limits

Content data binds quests to these events through simple hook records, for example:

- On item hand in: (NpcId, ItemId, QuestId)
- On say phrase match: (NpcId, text key, QuestId)
- On kill: (NpcTemplateId, QuestId, ObjectiveId)

Scripts only need to implement special edge cases; generic hooks handle the majority of standard content.

### 34.5 Integration With Other Systems

Quest resolution interacts with several core systems:

- Faction
    - Completing or failing a quest can apply faction deltas using the same delta format as kill based faction changes.
    - Some quests have minimum faction requirements before they can be offered.
- XP and rewards
    - Each quest defines experience reward, coin, and item choices.
    - Rewards can include spell scrolls, keys, flags, and temporary effect items.
- World and player flags
    - Boolean or small integer flags are stored per character to gate later quests, zone access, or dialogue options.
    - World level flags can drive unlocked events or phase changes for zones.
- Dialogue
    - NPC dialogue trees read quest state and flags in order to determine which responses are available.
    - Simple keyword based dialogue, similar to classic EverQuest, is supported first, with optional tree based systems later.

### 34.6 Zone Events and Scripts

Beyond quests, the same scripting infrastructure powers zone events. Examples include:

- Periodic spawn waves for an undead invasion
- Day and night spawn swaps
- Rare named spawn chance after a placeholder is killed a number of times
- Simple environmental events, such as bridges lowering or doors unlocking after a condition is met

Zone event definitions include:

- Trigger type (timer, kill count, quest state, world flag, time of day)
- Conditions (which zone, which NPCs, which player flags)
- Actions (spawn or despawn NPCs, broadcast text, toggle doors, grant or remove buffs)

Initially, simple zone events can be authored entirely in data. More complex cases can attach a script file that calls into a small API for spawn control and messaging.

Appendix A. Runtime Data Files

This appendix lists the core data files that drive REQ at runtime. All names are examples and can be adjusted, but the overall division helps keep content organized and consistent between server and client.

### A.1 Core Character and Progression Data

- races.json
Defines playable and non playable races, including display names, size tags, vision type, and references into race_data.json.
- race_data.json
Holds final numeric attribute values and racial traits for each race, including starting attributes and passive bonuses.
- classes.json
Defines classes, primary stats, resource usage, armor and weapon proficiencies, and spell or ability progression tables.
- xp_tables.json
Level based experience requirements for players and optionally separate tables for alternate advancement or future systems.

### A.2 Spells, Abilities, and Effects

- spells.json
Canonical spell list with ids, names, schools, cast times, costs, recast, resist type, and effect components.
- abilities.json
Non spell abilities such as combat arts, song lines, and racial actives, defined in a similar structure to spells.
- effect_templates.json
Optional shared definitions for recurring effect patterns, such as haste lines, damage over time lines, or strength buffs.

### A.3 Items, Loot, and Economy

- items.json
All equippable items, consumables, quest items, and crafting materials, including stats, restrictions, and value.
- loot_tables.json
Per NPC or per encounter loot definitions, with fixed drops, weighted random entries, and optional rare bonus tables.
- vendor_inventories.json
Per vendor listings that reference items.json, including stock counts, restock timers, and buy or sell modifiers.

### A.4 NPCs, Spawns, and AI

- npc_templates.json
Template records for NPC types, including race, class, level range, archetype, resist profile, base stats, and behavior flags.
- npc_spawns.json
Spawn points and groups for zones, including coordinates, respawn times, pathing sets, and references to npc_templates.json.
- path_grids.json
Waypoint lists for roamers and escort routes, referenced by npc_spawns.json and zone scripts.
- encounter_scripts.json
Optional data records for boss or event encounters that define phases and hook into the scripting system.

### A.5 Faction, Quests, and Dialogue

- factions.json
Faction identifiers, default standings by race and class, and relationships between factions for indirect gains and losses.
- quests.json
Quest definitions as described in section 19, including objectives, rewards, and links to faction deltas.
- quest_text.json
Localized or keyed text for journal entries, quest descriptions, and important quest related messages.
- dialogue.json
NPC dialogue lines and keyword mappings for simple talk interactions, including quest related branches.

### A.6 World and Zone Configuration

- world_config.json
Per world configuration, including world id, name, ruleset flags, QoL preset, and list of zones managed by the world server.
- zone_config_<zone>.json
Per zone configuration files that define climate, time of day rules, music, ambient settings, safe points, and zone wide flags.
- navigation_<zone>.json
Exported navmesh and movement parameters for server side pathing and movement validation.

### A.7 Technical and Administrative Data

- rulesets.json
Server side rule groups that control XP rates, death penalties, loot modifiers, and PvP settings.
- gm_commands.json
Configuration of game master commands and permission levels.
- logs_config.json
Logging categories and levels used by backend services.

These files cover the majority of runtime data needed for a vertical slice and for an EverQuest inspired emulator replacement. Additional tables can be added as new systems such as raids or advanced tradeskills come online.

## Appendix B. Timeboxed Development Roadmap (30–40 h/week)

This appendix reframes the vertical slice milestones from §15 around a single-developer implementation pace (Claude) with 30–40 hours/week of focused work, and a hard cap of 10 hours/week inside Unreal Editor.

The goal is an aggressive but realistic schedule to reach a fully playable vertical slice (Milestones 0–3) in roughly 6 calendar weeks.

### B.1 Team & Time Assumptions

- Total capacity: ~30–40 hrs/week of focused development.
- Unreal Editor cap: ≤10 hrs/week.
- Backend/tools time: ~20–30 hrs/week (C++ servers, data, validation, test harness).
- Roles:
  - Designer/Director: owns priorities, gameplay decisions, data files, and playtesting.
  - Implementation Engineer: owns C++ and Blueprint implementation in VS + Unreal.
  - Design Assistant (ChatGPT): provides breakdowns, pseudo-code, GDD updates, and refactors on demand.

All durations below are timeboxes, not estimates: if a milestone threatens to exceed its box, scope must be reduced.

### B.2 Six-Week Vertical Slice Overview

Mapping §15’s milestones onto this capacity:

Milestone	From §15	Focus	                                            Timebox	                        Unreal Editor Usage	                            Backend/Tools Usage
M0	        15.1	    Skeleton: login → world → zone, basic movement	    1 week	                        8–10 hrs (UI + net hookup)	                    20–25 hrs (servers + configs)
M1	        15.2	    First camp: combat loop, XP, simple NPCs	        2 weeks	                        8–9 hrs/week (HUD + combat UI)	                20–25 hrs/week (combat tuning)
M2	        15.3	    Grouping & small-scale social play	                1 week	                        8–10 hrs (party UI + chat)	                    20–25 hrs (group logic)
M3	        15.4	    Economy: items, loot, vendors, basic crafting	    2 weeks	                        8–9 hrs/week (inventory, vendors, crafting)	    20–25 hrs/week (inventory, loot, save/load)

Total vertical-slice runway: ~6 weeks of focused work under these constraints.

### B.3 Milestone 0 — Skeleton (1 week)

Goal (15.1):
Login → world select → character create/select → load into 1 zone, with basic movement working against the real server.

#### B.3.1 Scope

Backend & Tools (≈20–25 hrs)
- Ensure the refactored REQ_Backend builds cleanly in VS (all targets: Login, World, Zone, Shared, TestClient).
- Pick a single dev world + zone as the M0 target (e.g. “CazicThule / East Freeport”).
- Run the existing data validator on worlds/zones/NPCs/items and fix any reported config issues for that target world.
- Confirm, using the TestClient:
    - Account login
    - World list retrieval
    - Character list/create
    - Zone handoff and authentication
- Stabilize logs and error reporting so connection issues are obvious.

Unreal Editor (≤10 hrs)
- Implement a minimal network client layer that mirrors TestClient behavior:
    - Connect to LoginServer.
    - Send/receive the same auth + world/character/zone messages.
    - Expose events to Blueprint or a thin C++ game instance.
- Build “function-first” UMG screens:
    - LoginScreen – username/password, error text.
    - WorldSelectScreen – list worlds, “Enter World” button.
    - CharacterSelectScreen – list characters, “Create” / “Delete” / “Enter World”.
    - CharacterCreateScreen – name + very small subset of race/class options.
- On successful zone auth:
    - Load a simple greybox zone map.
    - Spawn the player at the server-defined safe point.
    - Implement basic local movement that is at least position-synced to the server.

Design Assistant (ChatGPT) usage
- Derive the exact message flow diagrams (Login ↔ World ↔ Zone) so Claude can mirror TestClient logic quickly.
- Help design the UMG flow and data structures.
- Help debug protocol mismatches by reasoning about logs/test output.

Acceptance criteria (M0 “done”)
- From a fresh client:
    - Log in, select the dev world, create/select a character.
    - Load into the zone and move around.
- Two local clients can be logged in and visible on the server (even if client-side visual replication is still simplistic).

### B.4 Milestone 1 — First Camp (2 weeks)

Goal (15.2):
1 outdoor zone with ~5 NPC types, a working combat loop (targeting → attack → damage → death), XP gain, and death/respawn.

Timeboxed to 2 weeks so combat/UI work can respect the 10 hrs/week UE cap.

#### B.4.1 Scope

Backend & Tools (≈20–25 hrs/week)

Week 2:

- Define the M1 camp in the dev zone:
    - 3–5 NPC archetypes (melee trash, weak caster, critter, etc.).
    - HP, damage, aggro radius, and respawn timers tuned for a solo/group experience.
- Ensure:
    - AttackRequest/AttackResult message paths are solid.
    - NPC HP and death are logged clearly.
    - XP awards use the relevant world rules table.
- Add simple flee / leash logic only if trivial; otherwise defer to later milestones.

Week 3:

- Balance NPC stats vs. starter player gear so fights are neither trivial nor impossible.
- Tighten any crash/edge cases discovered during playtesting (null targets, bad IDs, etc.).

Unreal Editor (≈8–9 hrs/week)

Week 2:

- Implement basic targeting:
    - Click-to-target and/or tab targeting.
    - Display a target frame (name + HP).
- Add a minimal HUD:
    - Player HP bar.
    - XP bar.
    - Target HP bar + name.
- Implement “auto-attack”:
    - Hotbar slot or keybinding that sends AttackRequest.
    - Show damage text and simple hit/miss feedback.

Week 3:

- Implement death & respawn visuals:
    - On 0 HP, play a death state (ragdoll or simple fall), fade, respawn at safe point.
- Add basic feedback:
    - Popups or log lines for XP gain and level-up.
    - Chat-style combat log window (text only).

Designer/Director responsibilities
- Define which NPCs live at the M1 camp, rough fantasy (skeletons/spiders/etc.), and their intended difficulty.
- Adjust their stats via JSON/data files with Claude’s help as needed.
- Playtest regularly, logging “too hard / too easy / broken” spots.

Acceptance criteria (M1 “done”)
- Two players can:
    - Run from spawn to the M1 camp.
    - Target and kill camp NPCs.
    - See damage, XP, and death/respawn behavior.
- Fights feel “EverQuest-like”: a bit dangerous, not button-mashy.

### B.5 Milestone 2 — Grouping (1 week)

Goal (15.3):
Introduce grouping, shared XP, and basic group UI to create real small-party play, without yet expanding into guilds or raids.

Timeboxed to 1 week with careful scope control.

#### B.5.1 Scope

Backend & Tools (≈20–25 hrs)
- Implement a lightweight Group model:
    - GroupId, leader, and list of member characterIds.
    - Operations: invite, accept, decline, leave, kick, disband.
- Wire group membership into the XP distribution system:
    - XP splits among group members within range, with a simple per-member bonus if desired.
- Add a basic group chat channel:
    - Tag messages as /group and route only to group members.
- Logging:
    - Clear logs for group creation, join/leave, and XP distribution events.

Unreal Editor (≤10 hrs)
- Implement group UI:
    - Party frame showing each member’s name, class, HP.
    - Simple indicator of group leader.
- Player interactions:
    - Right-click or slash command /invite <name>.
    - Accept/decline pop-up.
    - Leave group button.
- Chat:
    - Toggle between /say and /group in the chat window.
    - Add a basic prefix or color to group chat messages.

Designer/Director responsibilities
- Decide group size cap (e.g., 6 members like EQ).
- Define simple rules for who gets XP (range, tag rules, etc.).
- Playtest in the M1 camp with 2–4 characters to ensure XP feels fair and UI is readable.

Acceptance criteria (M2 “done”)
- Players can:
    - Form a group and see each other in party frames.
    - Share XP from kills.
    - Use /group chat distinct from /say.
- The group system is stable under joins/leaves during combat.

### B.6 Milestone 3 — Economy & Basic Crafting (2 weeks)

Goal (15.4):
Hook up the existing item/loot data so the vertical slice has kill → loot → sell → small craft loops: a simple but satisfying economy.

Timeboxed to 2 weeks to respect UE time and leave room for iteration.

#### B.6.1 Scope

Backend & Tools (≈20–25 hrs/week)

Week 5:
- Extend character data to include inventory and equipment slots, using the existing item templates and enums.
- Ensure inventory is saved/loaded with character data.
- Implement loot drops:
    - On NPC death, roll from per-zone or per-NPC loot tables.
    - Create simple “corpse” containers with item lists.
- Implement corpse cleanup rules (timeout/decay).

Week 6:
- Implement vendors:
    - Define 1–2 vendors in the dev zone with basic inventories.
    - Implement buy/sell with basic value rules.
- Implement a single simple crafting path:
    - One or two recipes (e.g., combine pelts into better armor).
    - Backend verification and failure messages.
- Hardening:
    - Ensure inventory operations are safe (no dupes, no negative counts).

Unreal Editor (≈8–9 hrs/week)

Week 5:
- Loot UI:
    - When interacting with a corpse, show a loot window with items.
    - “Loot All” + per-item loot.
    - Basic inventory panel (grid or list) with stack counts.
- Display item name, quality (via text or minimal styling), and whether it’s equipped.

Week 6:
- Vendor UI:
    - Vendor window with Buy/Sell tabs.
    - Display prices and player coin totals.
    - Basic tooltip with item stats.
- Crafting UI:
    - Minimal interface listing a few known recipes and a “Craft” button.
    - Show success/failure messages and update inventory accordingly.
- Small polish items:
    - Simple VFX or icon overlay for lootable corpses.
    - Level-up / item gain feedback tuning.

Designer/Director responsibilities
- Define which items drop in the M1 camp and their intended rarity.
- Configure vendor inventories and buy/sell pricing philosophy.
- Define 1–2 starter crafting recipes worth testing.

Acceptance criteria (M3 “done”)
- A fresh character can:
    - Kill NPCs at the camp, loot items and coins.
    - Go back to town, sell junk, buy an upgrade.
    - Perform at least one crafting recipe and equip the result.
- Core loops “feel MMO”: camp → loot → town → upgrade → back to camp.

### B.7 Suggested Weekly Cadence

To keep the 6-week schedule on track under Parkinson’s Law:
- Day 1 (1–2 hrs)
    - Designer/Director + Design Assistant: refine that week’s slice of the current milestone (exact features and “nice-to-haves”).
    - Implementation Engineer: break features into small tasks (backend vs UE).
- Days 1–3 (backend-heavy)
    - Focus primarily on C++ backend and data work (20–25 hrs for the   week).
    - Use TestClient and logs to validate behavior before touching UE.
- Days 4–5 (UE-heavy, ≤10 hrs in Editor)
    - Connect the week’s backend features into UMG/Blueprint/UI.
    - Playtest and log all issues for the next backend-heavy block.