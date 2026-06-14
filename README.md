# Insurgency Reforged

A Reforger adaptation of the classic Arma 2 Insurgency mission originally created by pogoman. Set on Everon, US forces against a USSR insurgent faction. Up to 32 players.

The core gameplay has not changed much from the original. A team of players pushes through towns, kills enemies, searches the bodies, and uses the intel gathered to locate and destroy hidden weapons caches. The mission ends when every cache is gone.

---

## Gameplay

### The basic loop

You start at a staging area with your team. The villages, towns and settlements across Everon are held by USSR insurgents. Your job is to clear them out, but clearing towns is only half the work. The real objective is finding and destroying the weapons caches hidden somewhere on the map.

The caches do not show up on the map at the start. You find them by gathering intel from dead enemies.

### Gathering intel

When you kill an enemy, walk up to the body and search it. There is a chance (not guaranteed) that the body carries intel. If it does, a marker appears on the map somewhere in the vicinity of one of the caches. Early markers are placed up to 2.5km away from the actual cache, so the first few will not tell you much on their own.

As your team continues searching bodies the markers get progressively closer to each cache. The game does not tell you which cache a marker belongs to or how many have been found for it. You have to watch the map, see where the markers are clustering, and work out which cache is in the middle.

Once enough intel has been collected for a specific cache, a notification goes out to the whole team that a location has been narrowed down. At that point you should have enough markers to make a reasonable guess about where to look.

### Destroying caches

Find the cache and destroy it. Once it goes up, all the markers tied to that cache clear from the map. If there are still caches remaining, the intel process continues for those. Repeat until all caches are destroyed.

That is the win condition. No time limit, just find and destroy the caches.

### Enemy pressure

The insurgents do not sit still. Several things are happening in the background that you will run into whether you go looking for them or not.

**Garrisons** - Towns have permanently placed defender groups. These do not respawn once cleared. The more towns your team works through, the quieter the map gets in those areas. Cache zones have more defenders than regular towns.

Flying over a town in a helicopter will not wake the garrison below. Players more than 50m above the ground are skipped during zone activation checks. Without this, one helicopter at altitude could kick off AI groups across half the map. Ambient patrols and vehicles still run as normal, so the area is not empty. The garrison activates as soon as a player drops below that altitude within range, so landing a helicopter in the zone still triggers it.

**Ambient patrols** - On top of the garrison, most towns have additional infantry patrols that do respawn after a long delay. These are there to keep cleared areas feeling active rather than completely empty.

**Ambient vehicles** - Enemy vehicles drive routes between towns along the road network. They engage any players they encounter. If the crew is killed the vehicle stays as a wreck. If no players are nearby when it arrives at its destination it simply despawns.

**Counter attacks** - Periodically, a squad will spawn outside a town where players are currently present and push in toward the centre. They will not just camp the edge; they move in. If players leave the town before the squad makes contact, the squad will abandon after a short wait. Once they are in contact they fight until wiped.

**Field threat** - Any group of players moving through open terrain between settlements will eventually attract a stalker patrol. It spawns out of sight, out of line of sight, and moves toward the group. Waypoints refresh on a timer so it keeps tracking even if the group moves. Getting in a vehicle and driving fast is one of the more reliable ways to shake one. Players inside a town or at base will not attract field threat patrols.

### Factions

The scenario supports up to 32 players. The USSR slot is capped at 8, leaving the majority of slots for the US side. Both factions can be played by real players, making this a PvPvE setup if you want it to be, though it works just as well as a straight co-op against the AI.

To enable PVP a GM must make the USSR faction playable. The USSR has pre defined spawn points around the terrain, quite a distance from towns, villages and settlements. Players on this side will spawn with predefined loadouts and have to work hard to locate human players.

---

## Server configuration

All settings below can be overridden by a server admin via the `missionHeader` block in the server config JSON. Paste this into the `game` section of your config. Any field you omit falls back to the default.

```json
"missionHeader": {
    "m_Insurgency": {
        "m_iCacheCount": 3,
        "m_iIntelThreshold": 10,
        "m_fIntelDropChance": 0.05,

        "m_bPatrolsEnabled": true,
        "m_iGroupsPerTown": 1,
        "m_iGroupsPerVillage": 1,
        "m_iGroupsPerSettlement": 2,
        "m_fPatrolRespawnDelay": 3600,

        "m_bGarrisonsEnabled": true,
        "m_iGarrisonCacheExtraGroups": 2,

        "m_bVehiclesEnabled": true,
        "m_iMaxActiveVehicles": 4,

        "m_bCounterAttacksEnabled": true,
        "m_iMaxConcurrentCounterAttacks": 2,
        "m_fCounterAttackInterval": 600,
        "m_iMinSquadsPerAttack": 1,
        "m_iMaxSquadsPerAttack": 3,
        "m_fZoneCooldown": 1200,

        "m_bFieldThreatEnabled": true,
        "m_iMaxConcurrentFieldPatrols": 3,
        "m_fThreatMinCooldown": 600,
        "m_fThreatMaxCooldown": 1200
    }
}
```

| Field | Default | Description |
|---|---|---|
| `m_iCacheCount` | 3 | Weapon caches that spawn at mission start. Header only, not in scenario properties. Caches are placed at startup. |
| `m_iIntelThreshold` | 10 | Intel items required to fully reveal each cache. |
| `m_fIntelDropChance` | 0.05 | Chance (0.0–1.0) that a searched body has intel on it. Default ratio is 1:20. |
| `m_bPatrolsEnabled` | true | Enable ambient infantry patrols. |
| `m_iGroupsPerTown` | 1 | Patrol groups per town. |
| `m_iGroupsPerVillage` | 1 | Patrol groups per village. |
| `m_iGroupsPerSettlement` | 2 | Patrol groups per small settlement. |
| `m_fPatrolRespawnDelay` | 3600 | Seconds before a cleared patrol zone respawns. |
| `m_bGarrisonsEnabled` | true | Enable garrison defenders. |
| `m_iGarrisonCacheExtraGroups` | 2 | Extra infantry groups added at garrison cache zones. |
| `m_bVehiclesEnabled` | true | Enable ambient vehicle patrols. |
| `m_iMaxActiveVehicles` | 4 | Maximum ambient enemy vehicles active at once. |
| `m_bCounterAttacksEnabled` | true | Enable counter-attack squads. |
| `m_iMaxConcurrentCounterAttacks` | 2 | Maximum counter-attack waves active simultaneously. |
| `m_fCounterAttackInterval` | 600 | Seconds between counter-attack attempts. |
| `m_iMinSquadsPerAttack` | 1 | Minimum squads per counter-attack wave. |
| `m_iMaxSquadsPerAttack` | 3 | Maximum squads per counter-attack wave. |
| `m_fZoneCooldown` | 1200 | Seconds before the same area can be counter-attacked again. 0 disables the cooldown. |
| `m_bFieldThreatEnabled` | true | Enable field threat patrols. |
| `m_iMaxConcurrentFieldPatrols` | 3 | Maximum field threat patrols active at once. |
| `m_fThreatMinCooldown` | 600 | Minimum seconds before a stalker patrol spawns on a group in the open. |
| `m_fThreatMaxCooldown` | 1200 | Maximum seconds before a stalker patrol spawns on a group in the open. |

Game Master scenario properties changed in session take priority over these values since they apply after the header is read at mission start.

---

# Systems and development

The rest of this document covers the individual systems, how they are structured, and how to integrate them into your own scenario. If you are just playing the mod you do not need any of this.

All of the values here are default, but can be modified to your liking. 

---

## Scripts and prefab locations

```
Scripts/Game/                  INS_MissionHeader.c, INS_ZoneHelpers.c
Scripts/Game/Actions/          INS_BodySearchAction.c
Scripts/Game/Components/       INS_AmbientZone.c, INS_BodySearchedComponent.c,
                               INS_CacheComponent.c, INS_CacheSpawnPoint.c,
                               INS_GarrisonZone.c, INS_SafeZone.c,
                               INS_VehicleRespawner.c
Scripts/Game/Managers/         All manager classes
Scripts/Game/Hooks/            INS_CacheDestructionHook.c
Scripts/Game/Editor/           Editor attribute classes

Prefabs/Props/                 INS_Cache_USSR.et
Prefabs/Characters/            Character_*_Base.et (base enemy prefab)
Prefabs/INS_CacheSpawnPoint.et
```

---

## Setting up your own scenario

### GameMode entity

All managers attach to your GameMode entity. Use whichever ones you need. None of them hard-require each other except for the cache/intel pair, which must be used together.

The components that need prefabs configured before they do anything:

| Component | Required attribute |
|---|---|
| `INS_GarrisonManager` | `m_aInfantryGroupPrefabs` |
| `INS_AmbientPatrolManager` | `m_aGroupPrefabs` |
| `INS_AmbientVehicleManager` | `m_aVehiclePrefabs` |
| `INS_CounterAttackManager` | `m_aSquadPrefabs` |
| `INS_FieldThreatManager` | `m_aGroupPrefabs` |
| `INS_CacheSpawnManager` | `m_sCachePrefab` |

`INS_IntelManager` also needs `SCR_HintManagerComponent` on the GameMode entity to broadcast messages to players.

### Enemy prefab setup

This step is easy to miss. Two things need adding to your enemy faction's base character prefab.

Open your enemy base character prefab in the Workbench (for example `Character_USSR_Base.et`):

1. Find the `ActionsManagerComponent` and add `INS_BodySearchAction` to it.
2. Add `INS_BodySearchedComponent` to the component list on the same prefab.

Every character variant that inherits from that base prefab picks both up automatically.

`INS_BodySearchedComponent` is what replicates the searched state to all clients. Without it the search action falls back to server-only state, which means other players nearby can still see "Search for Intel" on a body that has already been searched.

If you skip both, the cache system still works but players have no way to gather intel from bodies.

### World placement

**Garrison zones** - Place a `GenericEntity` with `INS_GarrisonZone` wherever you want a permanent defender group. Set `m_iGroupCount`, `m_fRadius`, and optionally `m_iVehicleCount`. Tick `m_bIsCacheZone` on zones that sit at a cache spawn point to get extra defenders there.

**Cache spawn points** - Place `GenericEntity` objects with `INS_CacheSpawnPoint` at locations where caches can potentially spawn. Put more of these than `m_iCacheCount`; the manager picks a random subset each session.

**Ambient patrol overrides** - `INS_AmbientPatrolManager` finds towns automatically from the map. To add a custom location or override the group count at a specific spot, place a `GenericEntity` with `INS_AmbientZone`. Any `INS_AmbientZone` within 100m of an auto-discovered zone replaces it.

**Vehicle route nodes** - `INS_AmbientVehicleManager` builds routes between auto-discovered locations. To add extra nodes (remote checkpoints, outposts), place a `GenericEntity` with `INS_AmbientZone` and tick `m_bIsVehicleNode`.

**Safe zones** - Place a `GenericEntity` with `INS_SafeZone` at your base or spawn area. `INS_FieldThreatManager` will not spawn stalker patrols inside it. You need at least one of these.

---

## Persistence

This system will likely change in the future but at the moment, three save files are written during a session:

| File | What it tracks |
|---|---|
| `$profile:INS_Saves/cache_positions.dat` | Which cache spawn points were chosen this session |
| `$profile:INS_Saves/garrison_cleared.dat` | Which garrison zones have been permanently cleared |
| `$profile:INS_Saves/intel_state.dat` | Intel counts, cache destroyed states, marker positions |

All three are deleted when the mission ends via `OnGameModeEnd`, so ending a mission via GM (faction victory/loss) clears everything. A server crash does not delete them, so on restart the session continues from where it left off.

If you encounter any issues, you can also manually delete the INS_Saves directory and start a new session to repopulate.

---

## Component reference

### INS_GarrisonManager

Attach to GameMode entity.

| Attribute | Default | Notes |
|---|---|---|
| `m_aInfantryGroupPrefabs` | (empty) | Required. Picked at random per spawn. |
| `m_aVehicleGroupPrefabs` | (empty) | Optional. |
| `m_fActivationRadius` | 600m | Zone activates when a player enters. |
| `m_fDeactivationRadius` | 800m | Zone despawns when all players leave. |
| `m_iCacheZoneExtraGroups` | 2 | Extra infantry groups added at cache zones. |
| `m_fPatrolWaypointInterval` | 60s | How often patrol waypoints rotate. |
| `m_iSpawnStaggerMs` | 1000ms | Delay between each group spawn to reduce frame spikes. |
| `m_fMaxActivationAltitude` | 50m | Players above this height are ignored (stops helicopter flyovers triggering zones). |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_AmbientPatrolManager

Attach to GameMode entity.

| Attribute | Default | Notes |
|---|---|---|
| `m_aGroupPrefabs` | (empty) | Required. |
| `m_fActivationRadius` | 600m | Zone activates when a player enters. |
| `m_fDeactivationRadius` | 800m | Zone despawns when all players leave. |
| `m_fRespawnDelay` | 3600s | Time before a cleared zone respawns. Scenario properties slider is capped at 7200 mission header accepts any value. |
| `m_fMinSpawnDistFromPlayer` | 500m | Groups will not spawn closer than this to any player. |
| `m_iGroupsPerTown` | 1 | Patrol groups spawned at town-sized locations. |
| `m_iGroupsPerVillage` | 1 | Patrol groups spawned at village-sized locations. |
| `m_iGroupsPerSettlement` | 2 | Patrol groups spawned at small settlements. |
| `m_fPatrolWaypointInterval` | 120s | How often patrol waypoints rotate. |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_AmbientVehicleManager

Attach to GameMode entity. Vehicle prefabs must have crew compartments configured.

| Attribute | Default | Notes |
|---|---|---|
| `m_aVehiclePrefabs` | (empty) | Required. |
| `m_iMaxActiveVehicles` | 4 | Total vehicles allowed on routes at once. |
| `m_fActivationRadius` | 800m | Route endpoint activation distance. |
| `m_fDeactivationRadius` | 1000m | Route endpoint deactivates when all players leave. |
| `m_fMinSpawnDistance` | 500m | Minimum distance from any player to spawn. |
| `m_fMaxRouteDistance` | 3000m | Maximum straight line distance between zones for a valid route. |
| `m_fMaxRoadSnapDistance` | 500m | Zones further than this from a road are skipped. |
| `m_iSpawnStaggerMs` | 500ms | Delay between each queued vehicle spawn when filling up to the active vehicle cap. |
| `m_fRerouteDelay` | 30s | How long a vehicle waits at its destination before turning around. |
| `m_fStuckTimeout` | 30s | Seconds without movement before the vehicle is despawned. |
| `m_fWreckLifetime` | 1800s | Seconds before a crew-killed wreck is removed. 0 = never. |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_CounterAttackManager

Attach to GameMode entity.

| Attribute | Default | Notes |
|---|---|---|
| `m_aSquadPrefabs` | (empty) | Required. |
| `m_fCounterAttackInterval` | 600s | Time between attack attempts. |
| `m_fSpawnDistance` | 500m | Distance from town centre to spawn point. |
| `m_iMaxConcurrentCounterAttacks` | 2 | Global cap on simultaneous attack waves. |
| `m_iMinSquadsPerAttack` | 1 | Minimum squads per attack wave. |
| `m_iMaxSquadsPerAttack` | 3 | Maximum squads per attack wave. |
| `m_fZoneCooldown` | 1200s | Time before the same town can be targeted again after an attack. |
| `m_fAbandonDelay` | 60s | Seconds before a pre-contact squad gives up and despawns. |
| `m_fEngagementRadius` | 300m | Distance at which a squad considers itself in contact with players. |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_FieldThreatManager

Attach to GameMode entity. Requires at least one `INS_SafeZone` in the world.

| Attribute | Default | Notes |
|---|---|---|
| `m_aGroupPrefabs` | (empty) | Required. |
| `m_fGroupRadius` | 200m | Players within this distance are treated as one group. |
| `m_fMinSpawnDist` | 300m | Inner edge of the spawn ring around the group. |
| `m_fMaxSpawnDist` | 500m | Outer edge of the spawn ring. |
| `m_fMinCooldown` | 600s | Minimum time before a patrol targets a group. |
| `m_fMaxCooldown` | 1200s | Maximum time. |
| `m_fFleeDistance` | 100m | If the group moves this far between ticks the patrol despawns (vehicle escape). |
| `m_fAbandonRange` | 600m | If the patrol drifts this far from the group it despawns. |
| `m_iMaxConcurrentPatrols` | 3 | Global cap across all player groups. |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_CacheSpawnManager

Attach to GameMode entity alongside `INS_IntelManager`.

| Attribute | Default | Notes |
|---|---|---|
| `m_sCachePrefab` | (empty) | Required. |
| `m_iCacheCount` | 3 | How many caches to place per session. |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_IntelManager

Attach to GameMode entity alongside `INS_CacheSpawnManager`.

| Attribute | Default | Notes |
|---|---|---|
| `m_iIntelThreshold` | 10 | Intel needed to fully reveal each cache. |
| `m_fIntelDropChance` | 0.05 | Chance (0.0–1.0) that a searched body has intel on it. |
| `m_iMarkerDistMax` | 2500m | Distance of the first marker from the cache. |
| `m_iMarkerDistMin` | 50m | Distance of the final marker. |
| `m_bDebugCacheMarkers` | false | Places markers at exact cache positions. Visible to admins only. Useful for testing. |
| `m_bDebugLog` | false | Verbose logging. |

---

### INS_BodySearchAction

Add to the `ActionsManagerComponent` on your enemy faction's base character prefab. No configurable attributes, drop chance is set globally on `INS_IntelManager`. Requires `INS_BodySearchedComponent` on the same prefab.

---

### INS_BodySearchedComponent

Add to the component list on your enemy faction's base character prefab alongside `INS_BodySearchAction`. No configurable attributes. Replicates the searched state so all clients see the action update correctly when a body is searched.

---

### INS_GarrisonZone

Attach to a `GenericEntity` placed in the world.

| Attribute | Default | Notes |
|---|---|---|
| `m_fRadius` | 150m | Scatter radius for group spawns. |
| `m_iGroupCount` | 2 | Infantry groups to spawn. |
| `m_bIsCacheZone` | false | Adds extra defender groups at this zone if a cache can spawn here. |
| `m_iVehicleCount` | 0 | Vehicle patrols to spawn (requires `m_aVehicleGroupPrefabs` on the manager). |

---

### INS_AmbientZone

Attach to a `GenericEntity` placed in the world.

| Attribute | Default | Notes |
|---|---|---|
| `m_fRadius` | 200m | Patrol radius. |
| `m_iGroupCount` | 2 | Infantry groups. Ignored if `m_bIsVehicleNode` is true. |
| `m_bIsVehicleNode` | false | When true, `INS_AmbientVehicleManager` uses this as a vehicle route node instead. |

---

### INS_SafeZone

Attach to a `GenericEntity` at your base or spawn area.

| Attribute | Default | Notes |
|---|---|---|
| `m_fRadius` | 300m | No field threat patrols will spawn inside this radius. |

---

### INS_CacheSpawnPoint

Attach to `GenericEntity` objects at potential cache locations. No attributes. The cache spawn manager picks a random subset of all registered points at session start.

---

### INS_VehicleRespawner

Attach to a `GenericEntity` at a vehicle spawn location. Not intended for AI enemy vehicles; use `INS_GarrisonManager` for those. Intended for friendly or mission-critical vehicles that need to come back after being destroyed.

| Attribute | Default | Notes |
|---|---|---|
| `m_sVehiclePrefab` | (empty) | Required. |
| `m_fRespawnDelay` | 60s | Wait after destruction before respawning. |
| `m_iMaxRespawns` | -1 | -1 = unlimited. |
| `m_fPollInterval` | 5s | How often to check if the vehicle is still alive. |
| `m_fWreckCleanupRadius` | 3m | Clears wreck entities near the spawn point before placing a new vehicle. |

---

## Thanks

GregOldGit, Biggus SuRiNkIUs, waspzie, and everyone from the 1st Royal Marines Commandos who put time into testing, sent feedback, or helped in any way. Genuinely appreciated.
