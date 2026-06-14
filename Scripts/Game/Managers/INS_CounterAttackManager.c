/**
 * INS_CounterAttackManager
 * by borked.gb
 *
 * Periodically spawns an enemy squad that pushes into a player occupied town,
 * creating a firefight with any players already there.
 *
 * Flow:
 *   1. Every m_fCounterAttackInterval seconds, find towns currently containing players.
 *   2. Filter out towns on cooldown (recently attacked).
 *   3. Pick one at random and find a spawn point ~m_fSpawnDistance away with
 *      terrain blocking line of sight back to the town centre.
 *   4. Spawn the squad and assign a Move waypoint to the town centre.
 *   5. They push in and engage.
 *   6. If players leave before contact, the squad abandons after m_fAbandonDelay seconds.
 *   7. Once the squad is wiped the slot is freed and the town goes on cooldown.
 */

// A town/village zone tracked by the counter-attack system.
class INS_CounterAttackZone
{
	vector m_vCenter;
	float  m_fRadius;      // player detection radius
	string m_sName;
	bool   m_bOnCooldown;

	void INS_CounterAttackZone(vector center, float radius, string name)
	{
		m_vCenter    = center;
		m_fRadius    = radius;
		m_sName      = name;
		m_bOnCooldown = false;
	}
}

// One active counter-attack squad.
class INS_CounterAttackSquad
{
	IEntity     m_GroupEntity;
	SCR_AIGroup m_Group;
	IEntity     m_CurrentWaypoint;
	INS_CounterAttackZone m_Zone;         // which zone this squad is heading to
	bool        m_bActive;
	bool        m_bEverHadLiving;
	bool        m_bEngaged;      // true once a player has come within engagement range
	float       m_fNoPlayerTimer; // seconds since last player was nearby (pre-engagement only)

	void INS_CounterAttackSquad(IEntity groupEnt, INS_CounterAttackZone zone)
	{
		m_GroupEntity    = groupEnt;
		m_Group          = SCR_AIGroup.Cast(groupEnt);
		m_CurrentWaypoint = null;
		m_Zone           = zone;
		m_bActive        = true;
		m_bEverHadLiving = false;
		m_bEngaged       = false;
		m_fNoPlayerTimer = 0;
	}
}

[ComponentEditorProps(category: "Insurgency", description: "Periodically sends counter-attack squads into player-occupied towns. Attach to GameMode entity.")]
class INS_CounterAttackManagerClass : SCR_BaseGameModeComponentClass {}

class INS_CounterAttackManager : SCR_BaseGameModeComponent
{
	[Attribute()]
	protected ref array<ResourceName> m_aSquadPrefabs;

	[Attribute("600", UIWidgets.EditBox, "Seconds between counter-attack attempts.")]
	protected float m_fCounterAttackInterval;

	[Attribute("500", UIWidgets.EditBox, "Spawn distance (m) from the target town centre.")]
	protected float m_fSpawnDistance;

	[Attribute("2", UIWidgets.EditBox, "Maximum concurrent counter-attack squads.")]
	protected int m_iMaxConcurrentCounterAttacks;

	[Attribute("1", UIWidgets.EditBox, "Minimum number of squads spawned per counter-attack.")]
	protected int m_iMinSquadsPerAttack;

	[Attribute("3", UIWidgets.EditBox, "Maximum number of squads spawned per counter-attack.")]
	protected int m_iMaxSquadsPerAttack;

	[Attribute("1200", UIWidgets.EditBox, "Seconds before the same town can be targeted again.")]
	protected float m_fZoneCooldown;

	[Attribute("60", UIWidgets.EditBox, "Seconds without a player nearby before a pre-contact squad abandons.")]
	protected float m_fAbandonDelay;

	[Attribute("300", UIWidgets.EditBox, "Distance (m) at which a player is considered in contact with the squad.")]
	protected float m_fEngagementRadius;

	[Attribute("15", UIWidgets.EditBox, "Seconds between squad monitor checks.")]
	protected float m_fCheckInterval;

	[Attribute("400", UIWidgets.EditBox, "Milliseconds between each queued squad spawn. Spread spawns out to avoid frame spikes.")]
	protected int m_iSpawnStaggerMs;

	protected static const int   TYPE_TOWN       = 59;
	protected static const int   TYPE_VILLAGE    = 60;
	protected static const int   TYPE_SETTLEMENT = 61;
	protected static const float RADIUS_TOWN     = 350;
	protected static const float RADIUS_VILLAGE  = 250;
	protected static const float RADIUS_SETTLEMENT = 175;

	protected bool m_bEnabled = true;
	protected bool m_bJIPPause = false;
	protected ref ScriptCallQueue m_CallQueue = new ScriptCallQueue();

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (spawns, squad events, cooldowns).")]
	protected bool m_bDebugLog = false;

	protected ref array<ref INS_CounterAttackZone>  m_aZones        = {};
	protected ref array<ref INS_CounterAttackSquad> m_aActiveSquads = {};

	// Temp storage during zone discovery
	protected ref array<ref INS_CounterAttackZone> m_aTempZones;

	// Public accessors
	bool  IsEnabled()                              { return m_bEnabled; }
	int   GetMaxConcurrentCounterAttacks()         { return m_iMaxConcurrentCounterAttacks; }
	float GetCounterAttackInterval()               { return m_fCounterAttackInterval; }
	int   GetMinSquadsPerAttack()                  { return m_iMinSquadsPerAttack; }
	int   GetMaxSquadsPerAttack()                  { return m_iMaxSquadsPerAttack; }
	float GetZoneCooldown()                        { return m_fZoneCooldown; }
	void  SetMaxConcurrentCounterAttacks(int count){ m_iMaxConcurrentCounterAttacks = count; }
	void  SetCounterAttackInterval(float seconds)  { m_fCounterAttackInterval = seconds; }
	void  SetMinSquadsPerAttack(int count)         { m_iMinSquadsPerAttack = count; }
	void  SetMaxSquadsPerAttack(int count)         { m_iMaxSquadsPerAttack = count; }
	void  SetZoneCooldown(float seconds)           { m_fZoneCooldown = seconds; }

	void SetEnabled(bool bEnabled)
	{
		if (m_bEnabled == bEnabled)
			return;

		m_bEnabled = bEnabled;

		if (!Replication.IsServer())
			return;

		if (!bEnabled)
		{
			m_CallQueue.Remove(CounterAttackTick);
			m_CallQueue.Remove(MonitorTick);
			for (int i = m_aActiveSquads.Count() - 1; i >= 0; i--)
				DespawnSquad(m_aActiveSquads[i]);
		}
		else
		{
			m_CallQueue.CallLater(CounterAttackTick,    (int)(m_fCounterAttackInterval  * 1000), true);
			m_CallQueue.CallLater(MonitorTick, (int)(m_fCheckInterval * 1000), true);
		}
	}

	protected void ApplyMissionHeader()
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return;

		INS_MissionParams params = header.m_Insurgency;
		if (!params)
			return;

		m_bEnabled                     = params.m_bCounterAttacksEnabled;
		m_iMaxConcurrentCounterAttacks = params.m_iMaxConcurrentCounterAttacks;
		m_fCounterAttackInterval       = params.m_fCounterAttackInterval;
		m_iMinSquadsPerAttack          = params.m_iMinSquadsPerAttack;
		m_iMaxSquadsPerAttack          = params.m_iMaxSquadsPerAttack;
		m_fZoneCooldown                = params.m_fZoneCooldown;
	}

	// Lifecycle
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();

		if (!m_aSquadPrefabs || m_aSquadPrefabs.IsEmpty())
		{
			Print("[INS_CounterAttackManager] WARNING: No squad prefabs set. Disabled.");
			return;
		}

		DiscoverZones();

		if (m_aZones.IsEmpty())
		{
			Print("[INS_CounterAttackManager] WARNING: No zones found. Disabled.");
			return;
		}

		Print(string.Format("[INS_CounterAttackManager] Started. %1 zone(s) available.", m_aZones.Count()));

		GetGame().GetCallqueue().CallLater(TickCallQueue, 100, true);

		if (m_bEnabled)
		{
			m_CallQueue.CallLater(CounterAttackTick, (int)(m_fCounterAttackInterval * 1000), true);
			m_CallQueue.CallLater(MonitorTick, (int)(m_fCheckInterval * 1000), true);
		}
	}

	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().Remove(TickCallQueue);

		for (int i = m_aActiveSquads.Count() - 1; i >= 0; i--)
			DespawnSquad(m_aActiveSquads[i]);
	}

	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		if (!Replication.IsServer())
			return;

		m_bJIPPause = true;
		m_CallQueue.Remove(ClearJIPPause);
		m_CallQueue.CallLater(ClearJIPPause, 15000, false);
	}

	protected void ClearJIPPause()
	{
		m_bJIPPause = false;
	}

	protected void TickCallQueue()
	{
		m_CallQueue.Tick(0.1);
	}

	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (!Replication.IsServer())
			return;

		array<int> remaining = {};
		GetGame().GetPlayerManager().GetPlayers(remaining);
		if (!remaining.IsEmpty())
			return;

		Print("[INS_CounterAttackManager] Last player left, despawning all counter-attack squads.");
		for (int i = m_aActiveSquads.Count() - 1; i >= 0; i--)
			DespawnSquad(m_aActiveSquads[i]);
	}

	// Zone discovery. Same map query used by the other ambient managers.
	protected void DiscoverZones()
	{
		m_aTempZones = new array<ref INS_CounterAttackZone>();

		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		GetGame().GetWorld().QueryEntitiesByAABB(mins, maxs, OnZoneFound);

		m_aZones     = m_aTempZones;
		m_aTempZones = null;
	}

	protected bool OnZoneFound(IEntity entity)
	{
		MapDescriptorComponent mapDesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!mapDesc)
			return true;

		int descType = mapDesc.GetBaseType();
		if (descType != TYPE_TOWN && descType != TYPE_VILLAGE && descType != TYPE_SETTLEMENT)
			return true;

		string locName = string.Empty;
		MapItem item = mapDesc.Item();
		if (item)
			locName = item.GetDisplayName();

		if (INS_ZoneHelpers.IsExcludedName(locName))
			return true;

		float radius;
		if (descType == TYPE_TOWN)
			radius = RADIUS_TOWN;
		else if (descType == TYPE_VILLAGE)
			radius = RADIUS_VILLAGE;
		else
			radius = RADIUS_SETTLEMENT;

		m_aTempZones.Insert(new INS_CounterAttackZone(entity.GetOrigin(), radius, locName));

		return true;
	}

	// Counter attack tick. Fires every m_fCounterAttackInterval seconds.
	protected void CounterAttackTick()
	{
		if (!Replication.IsServer())
			return;

		if (m_aActiveSquads.Count() >= m_iMaxConcurrentCounterAttacks)
			return;

		if (m_bJIPPause)
			return;

		array<vector> playerPos = GetPlayerPositions();
		if (playerPos.IsEmpty())
			return;

		// Find zones that have players inside and aren't on cooldown
		array<INS_CounterAttackZone> eligible = {};
		foreach (INS_CounterAttackZone zone : m_aZones)
		{
			if (zone.m_bOnCooldown)
				continue;

			float zoneRadiusSq = zone.m_fRadius * zone.m_fRadius;
			foreach (vector pPos : playerPos)
			{
				if (vector.DistanceSq(zone.m_vCenter, pPos) <= zoneRadiusSq)
				{
					eligible.Insert(zone);
					break;
				}
			}
		}

		if (eligible.IsEmpty())
		{
			if (m_bDebugLog)
				Print("[INS_CounterAttackManager] Counter attack tick: no eligible zones with players.");
			return;
		}

		INS_CounterAttackZone target = eligible[Math.RandomInt(0, eligible.Count())];

		int slotsAvailable = m_iMaxConcurrentCounterAttacks - m_aActiveSquads.Count();
		int safeMin        = Math.Clamp(m_iMinSquadsPerAttack, 1, 5);
		int safeMax        = Math.Clamp(m_iMaxSquadsPerAttack, safeMin, 5);
		int squadCount     = Math.RandomInt(safeMin, safeMax + 1);
		squadCount         = Math.Min(squadCount, slotsAvailable);

		int spawned = 0;
		for (int s = 0; s < squadCount; s++)
		{
			vector spawnPos = FindSpawnPosition(target.m_vCenter, m_fSpawnDistance);
			if (spawnPos == vector.Zero)
			{
				if (m_bDebugLog)
					Print(string.Format("[INS_CounterAttackManager] Could not find spawn position for squad %1/%2 on '%3'.", s + 1, squadCount, target.m_sName));
				continue;
			}

			SpawnCounterAttackSquad(spawnPos, target);
			spawned++;
		}

		if (spawned == 0)
			return;

		// Put the zone on cooldown
		target.m_bOnCooldown = true;
		m_CallQueue.CallLater(ClearZoneCooldown, (int)(m_fZoneCooldown * 1000), false, target);

		if (m_bDebugLog)
			Print(string.Format("[INS_CounterAttackManager] Counter attack on '%1': %2 squad(s) spawned.", target.m_sName, spawned));
	}

	protected void ClearZoneCooldown(INS_CounterAttackZone zone)
	{
		zone.m_bOnCooldown = false;
		if (m_bDebugLog)
			Print(string.Format("[INS_CounterAttackManager] '%1' cooldown cleared.", zone.m_sName));
	}

	// Monitor tick. Checks active squads every m_fCheckInterval seconds.
	protected void MonitorTick()
	{
		if (!Replication.IsServer())
			return;

		array<vector> playerPos = GetPlayerPositions();
		array<AIAgent> agents = {};

		for (int i = m_aActiveSquads.Count() - 1; i >= 0; i--)
		{
			INS_CounterAttackSquad squad = m_aActiveSquads[i];

			if (!squad.m_bActive || !squad.m_GroupEntity)
			{
				m_aActiveSquads.Remove(i);
				continue;
			}

			// Count living members it seems agents persist in the group after death, so check the damage manager
			int living = 0;
			if (squad.m_Group)
			{
				agents.Clear();
				squad.m_Group.GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					IEntity agentEnt = agent.GetControlledEntity();
					if (!agentEnt)
						continue;
					DamageManagerComponent dmg = DamageManagerComponent.Cast(agentEnt.FindComponent(DamageManagerComponent));
					if (dmg && !dmg.IsDestroyed())
						living++;
				}
			}

			if (living > 0)
				squad.m_bEverHadLiving = true;

			// Squad wiped out
			if (squad.m_bEverHadLiving && living == 0)
			{
				if (m_bDebugLog)
					Print(string.Format("[INS_CounterAttackManager] Counter attack squad on '%1' eliminated.", squad.m_Zone.m_sName));
				DespawnSquad(squad);
				continue;
			}

			// Pre-contact: check if any player is still in the zone
			if (!squad.m_bEngaged)
			{
				bool playerInZone = false;
				float squadZoneRadiusSq = squad.m_Zone.m_fRadius * squad.m_Zone.m_fRadius;
				foreach (vector pPos : playerPos)
				{
					if (vector.DistanceSq(squad.m_Zone.m_vCenter, pPos) <= squadZoneRadiusSq)
					{
						playerInZone = true;
						break;
					}
				}

				if (!playerInZone)
				{
					squad.m_fNoPlayerTimer += m_fCheckInterval;
					if (squad.m_fNoPlayerTimer >= m_fAbandonDelay)
					{
						if (m_bDebugLog)
							Print(string.Format("[INS_CounterAttackManager] Counter attack squad on '%1' abandoned. No players in zone.", squad.m_Zone.m_sName));
						DespawnSquad(squad);
						continue;
					}
				}
				else
				{
					squad.m_fNoPlayerTimer = 0;

					// Check if we're now in contact
					vector squadPos = squad.m_GroupEntity.GetOrigin();
					float engageSq = m_fEngagementRadius * m_fEngagementRadius;
					foreach (vector pPos : playerPos)
					{
						if (vector.DistanceSq(squadPos, pPos) <= engageSq)
						{
							squad.m_bEngaged = true;
							if (m_bDebugLog)
								Print(string.Format("[INS_CounterAttackManager] Counter attack squad on '%1' in contact.", squad.m_Zone.m_sName));
							break;
						}
					}
				}
			}
		}
	}

	// Finds a point ~spawnDist from zone centre with terrain blocking line of sight back to it.
	protected vector FindSpawnPosition(vector zoneCenter, float spawnDist)
	{
		BaseWorld world = GetGame().GetWorld();
		float zoneY = world.GetSurfaceY(zoneCenter[0], zoneCenter[2]);
		vector zoneLook = Vector(zoneCenter[0], zoneY + 1.5, zoneCenter[2]);

		const int MAX_ATTEMPTS = 30;
		for (int i = 0; i < MAX_ATTEMPTS; i++)
		{
			float angle = Math.RandomFloat01() * Math.PI * 2.0;
			float x     = zoneCenter[0] + spawnDist * Math.Cos(angle);
			float z     = zoneCenter[2] + spawnDist * Math.Sin(angle);
			float y     = world.GetSurfaceY(x, z);

			if (y <= 0.5)
				continue; // water

			vector candidate = Vector(x, y + 1.5, z);

			TraceParam trace = new TraceParam();
			trace.Start  = candidate;
			trace.End    = zoneLook;
			trace.Flags  = TraceFlags.WORLD;

			float t = world.TraceMove(trace, null);
			if (t < 1.0)
				return Vector(x, y, z); // terrain blocks LoS, good spawn point
		}

		// Fallback: no LoS guarantee, just pick a land position at the right distance
		if (m_bDebugLog)
			Print(string.Format("[INS_CounterAttackManager] WARNING: Could not find LoS-blocked position for '%1'. Using fallback.", zoneCenter.ToString()));
		for (int i = 0; i < 10; i++)
		{
			float angle = Math.RandomFloat01() * Math.PI * 2.0;
			float x     = zoneCenter[0] + spawnDist * Math.Cos(angle);
			float z     = zoneCenter[2] + spawnDist * Math.Sin(angle);
			float y     = world.GetSurfaceY(x, z);

			if (y > 0.5)
				return Vector(x, y, z);
		}

		return vector.Zero; // complete failure, caller will skip this counter attack
	}

	// Squad spawning
	protected void SpawnCounterAttackSquad(vector spawnPos, INS_CounterAttackZone zone)
	{
		ResourceName prefab = m_aSquadPrefabs[Math.RandomInt(0, m_aSquadPrefabs.Count())];
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_CounterAttackManager] ERROR: Cannot load squad prefab: " + prefab);
			return;
		}

		// Face toward the town
		vector dir = zone.m_vCenter - spawnPos;
		dir[1] = 0;
		dir.Normalize();

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = spawnPos;
		params.Transform[2] = dir;

		vector up    = Vector(0, 1, 0);
		vector right = up * dir;
		params.Transform[0] = right;
		params.Transform[1] = up;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			Print("[INS_CounterAttackManager] ERROR: SpawnEntityPrefab returned null.");
			return;
		}

		// Fresh squads spawn on restart anyway, nothing useful to save here.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
			persistence.StopTracking(ent, true);

		INS_CounterAttackSquad squad = new INS_CounterAttackSquad(ent, zone);
		m_aActiveSquads.Insert(squad);

		m_CallQueue.CallLater(InitSquad,       500,  false, squad);
		m_CallQueue.CallLater(AssignWaypoint, 1500,  false, squad, zone.m_vCenter);
	}

	protected void InitSquad(INS_CounterAttackSquad squad)
	{
		if (!squad.m_bActive || !squad.m_GroupEntity)
			return;

		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(squad.m_GroupEntity);
		if (aiGroup)
			aiGroup.SpawnAllImmediately();
	}

	protected void AssignWaypoint(INS_CounterAttackSquad squad, vector destination)
	{
		if (!squad.m_bActive || !squad.m_Group)
			return;

		Resource wpRes = Resource.Load(INS_MOVE_WAYPOINT_PREFAB);
		if (!wpRes || !wpRes.IsValid())
		{
			Print("[INS_CounterAttackManager] ERROR: Cannot load move waypoint prefab.");
			DespawnSquad(squad);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = destination;

		IEntity wpEnt = GetGame().SpawnEntityPrefab(wpRes, GetGame().GetWorld(), params);
		if (!wpEnt)
		{
			Print("[INS_CounterAttackManager] ERROR: Failed to spawn waypoint.");
			DespawnSquad(squad);
			return;
		}

		AIWaypoint wp = AIWaypoint.Cast(wpEnt);
		if (!wp)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(wpEnt);
			DespawnSquad(squad);
			return;
		}

		squad.m_Group.AddWaypoint(wp);
		squad.m_CurrentWaypoint = wpEnt;

		if (m_bDebugLog)
			Print(string.Format("[INS_CounterAttackManager] Squad waypoint set. Pushing toward '%1'.", squad.m_Zone.m_sName));
	}

	// Despawn
	protected void DespawnSquad(INS_CounterAttackSquad squad)
	{
		squad.m_bActive = false;

		if (squad.m_CurrentWaypoint)
		{
			AIWaypoint wp = AIWaypoint.Cast(squad.m_CurrentWaypoint);
			if (wp && squad.m_Group)
				squad.m_Group.RemoveWaypoint(wp);

			SCR_EntityHelper.DeleteEntityAndChildren(squad.m_CurrentWaypoint);
			squad.m_CurrentWaypoint = null;
		}

		if (squad.m_GroupEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(squad.m_GroupEntity);

		m_aActiveSquads.RemoveItem(squad);
	}

	// Helpers
	protected array<vector> GetPlayerPositions()
	{
		array<vector> positions = {};
		array<int>    playerIds = {};

		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (controlled)
				positions.Insert(controlled.GetOrigin());
		}

		return positions;
	}

}
