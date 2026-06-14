/**
 * INS_AmbientPatrolManager
 * by borked.gb
 *
 * Spawns ambient infantry patrols at towns, villages, and settlements discovered
 * automatically from the map. Unlike INS_GarrisonManager which uses manualy placed
 * zones that clear permanently, zones here respawn enemies after a delay. They are
 * ambient filler, not objectives.
 *
 * Zone lifecycle:
 *   - Activates when a player enters m_fActivationRadius and the respawn timer has expired.
 *   - If saved positions exist from a previous activation, groups spawn there.
 *   - Deactivates when all players leave m_fDeactivationRadius. Current positions are saved.
 *   - When all enemies are killed a respawn timer starts. After m_fRespawnDelay seconds
 *     the zone activates again with fresh groups at random positions.
 *
 * Designer overrides:
 *   Place INS_AmbientZone components on GenericEntities to add zones auto-discovery misses,
 *   or to override group counts at specific auto-discovered locations.
 *
 * Setup:
 *   Attach to the GameMode entity. Set m_aGroupPrefabs to enemy SCR_AIGroup prefabs.
 *
 * Extra Info:
 * This works in an almost identical way to my ambient civilians mod, check the modpage for info.
 */

// Per zone runtime state for the ambient patrol system.
class INS_AmbientPatrolZoneState
{
	vector   m_vCenter;
	float    m_fRadius;
	int      m_iGroupCount;
	int      m_iDescType;         // map descriptor type (59/60/61), or 0 for town zones
	bool     m_bActive;
	bool     m_bReadyToRespawn;   // false while the respawn timer is counting down
	bool     m_bEverHadLiving;    // guards against clearing before members have spawned
	ref array<vector>             m_aSavedPositions;
	ref array<ref INS_PatrolAgent> m_aAgents;         // reuses INS_PatrolAgent from INS_GarrisonManager

	void INS_AmbientPatrolZoneState(vector center, float radius, int groupCount, int descType = 0)
	{
		m_vCenter         = center;
		m_fRadius         = radius;
		m_iGroupCount     = groupCount;
		m_iDescType       = descType;
		m_bActive         = false;
		m_bReadyToRespawn = true;
		m_bEverHadLiving  = false;
		m_aSavedPositions = new array<vector>();
		m_aAgents         = new array<ref INS_PatrolAgent>();
	}
}

[ComponentEditorProps(category: "Insurgency", description: "Infantry patrols at towns, villages and settlements, found automatically from the map. Enemies come back after a delay.")]
class INS_AmbientPatrolManagerClass : SCR_BaseGameModeComponentClass {}

class INS_AmbientPatrolManager : SCR_BaseGameModeComponent
{
	[Attribute()]
	protected ref array<ResourceName> m_aGroupPrefabs;

	[Attribute("600", UIWidgets.EditBox, "Distance (m) at which a zone activates.")]
	protected float m_fActivationRadius;

	[Attribute("800", UIWidgets.EditBox, "Distance (m) at which an active zone deactivates.")]
	protected float m_fDeactivationRadius;

	[Attribute("5", UIWidgets.EditBox, "Seconds between proximity checks.")]
	protected float m_fCheckInterval;

	[Attribute("120", UIWidgets.EditBox, "Seconds between patrol waypoint replacements.")]
	protected float m_fPatrolWaypointInterval;

	[Attribute("1000", UIWidgets.EditBox, "Milliseconds between each queued group spawn. Spread spawns out to avoid frame spikes.")]
	protected int m_iSpawnStaggerMs;

	[Attribute("3600", UIWidgets.EditBox, "Seconds before a cleared zone respawns enemies.")]
	protected float m_fRespawnDelay;

	[Attribute("500", UIWidgets.EditBox, "Minimum distance (m) from any player when spawning a group. If no safe position is found the spawn is skipped.")]
	protected float m_fMinSpawnDistFromPlayer;

	[Attribute("1", UIWidgets.EditBox, "Infantry groups per town (map type 59).")]
	protected int m_iGroupsPerTown;

	[Attribute("1", UIWidgets.EditBox, "Infantry groups per village (map type 60).")]
	protected int m_iGroupsPerVillage;

	[Attribute("2", UIWidgets.EditBox, "Infantry groups per settlement (map type 61).")]
	protected int m_iGroupsPerSettlement;

	protected static const int   TYPE_TOWN              = 59;
	protected static const int   TYPE_VILLAGE           = 60;
	protected static const int   TYPE_SETTLEMENT        = 61;
	protected static const float RADIUS_TOWN            = 150;   // matches AC civilian defaults
	protected static const float RADIUS_VILLAGE         = 100;
	protected static const float RADIUS_SETTLEMENT      = 200;
	protected static const float DESIGNER_MERGE_DIST    = 100; // designer zone replaces auto zone within this range

	protected bool m_bEnabled = true;
	protected bool m_bJIPPause = false;
	protected ref ScriptCallQueue m_CallQueue = new ScriptCallQueue();

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (spawns, activations, deactivations).")]
	protected bool m_bDebugLog = false;

	protected ref array<ref INS_AmbientPatrolZoneState> m_aZones = {};

	// Temp storage during zone discovery, cleared after DiscoverZones() returns
	protected ref array<ref INS_AmbientPatrolZoneState> m_aTempZones;

	// Public accessors. Used by editor attributes and mission header.
	bool IsEnabled() { return m_bEnabled; }

	int   GetGroupsPerTown()        { return m_iGroupsPerTown; }
	int   GetGroupsPerVillage()     { return m_iGroupsPerVillage; }
	int   GetGroupsPerSettlement()  { return m_iGroupsPerSettlement; }
	float GetRespawnDelay()         { return m_fRespawnDelay; }
	void  SetRespawnDelay(float seconds) { m_fRespawnDelay = seconds; }

	void SetGroupsPerTown(int count)
	{
		m_iGroupsPerTown = count;
		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			if (!state.m_bActive && state.m_iDescType == TYPE_TOWN)
				state.m_iGroupCount = count;
		}
	}

	void SetGroupsPerVillage(int count)
	{
		m_iGroupsPerVillage = count;
		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			if (!state.m_bActive && state.m_iDescType == TYPE_VILLAGE)
				state.m_iGroupCount = count;
		}
	}

	void SetGroupsPerSettlement(int count)
	{
		m_iGroupsPerSettlement = count;
		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			if (!state.m_bActive && state.m_iDescType == TYPE_SETTLEMENT)
				state.m_iGroupCount = count;
		}
	}

	// Returns true if pos falls inside any currently active patrol zone (used by INS_FieldThreatManager).
	bool IsPositionInActiveZone(vector pos)
	{
		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			if (!state.m_bActive)
				continue;
			float rSq = state.m_fRadius * state.m_fRadius;
			if (vector.DistanceSq(pos, state.m_vCenter) <= rSq)
				return true;
		}
		return false;
	}

	void SetEnabled(bool bEnabled)
	{
		if (m_bEnabled == bEnabled)
			return;

		m_bEnabled = bEnabled;

		if (!Replication.IsServer())
			return;

		if (!bEnabled)
		{
			m_CallQueue.Remove(TickProximityCheck);
			foreach (INS_AmbientPatrolZoneState state : m_aZones)
			{
				if (state.m_bActive)
					DeactivateZone(state);
			}
		}
		else
		{
			m_CallQueue.CallLater(TickProximityCheck, (int)(m_fCheckInterval * 1000), true);
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

		m_bEnabled                = params.m_bPatrolsEnabled;
		m_iGroupsPerTown          = params.m_iGroupsPerTown;
		m_iGroupsPerVillage       = params.m_iGroupsPerVillage;
		m_iGroupsPerSettlement    = params.m_iGroupsPerSettlement;
		m_fRespawnDelay           = params.m_fPatrolRespawnDelay;
	}

	// Lifecycle
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();

		if (!m_aGroupPrefabs || m_aGroupPrefabs.IsEmpty())
		{
			Print("[INS_AmbientPatrolManager] WARNING: No group prefabs set — disabled.");
			return;
		}

		DiscoverZones();

		Print(string.Format("[INS_AmbientPatrolManager] Started — %1 patrol zones.", m_aZones.Count()));

		GetGame().GetCallqueue().CallLater(TickCallQueue, 100, true);
		if (m_bEnabled)
			m_CallQueue.CallLater(TickProximityCheck, (int)(m_fCheckInterval * 1000), true);
	}

	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().Remove(TickCallQueue);

		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			if (state.m_bActive)
				DeactivateZone(state);
		}
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

		Print("[INS_AmbientPatrolManager] Last player left, deactivating all patrol zones.");
		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			if (state.m_bActive)
				DeactivateZone(state);
		}
	}

	// Zone discovery
	protected void DiscoverZones()
	{
		m_aTempZones = new array<ref INS_AmbientPatrolZoneState>();

		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		GetGame().GetWorld().QueryEntitiesByAABB(mins, maxs, OnLocationFound);

		// Merge designer-placed INS_AmbientZone components
		array<INS_AmbientZone> designerZones = {};
		INS_AmbientZone.CollectZones(designerZones);

		foreach (INS_AmbientZone dz : designerZones)
		{
			IEntity dzOwner = dz.GetOwner();
			if (!dzOwner)
				continue;

			vector dzCenter = dzOwner.GetOrigin();
			bool replaced = false;

			// If within merge range of an auto-discovered zone, override it
			for (int i = 0; i < m_aTempZones.Count(); i++)
			{
				vector diff = m_aTempZones[i].m_vCenter - dzCenter;
				if (diff.Length() <= DESIGNER_MERGE_DIST)
				{
					m_aTempZones[i] = new INS_AmbientPatrolZoneState(dzCenter, dz.GetRadius(), dz.GetGroupCount());
					replaced = true;
					break;
				}
			}

			if (!replaced)
				m_aTempZones.Insert(new INS_AmbientPatrolZoneState(dzCenter, dz.GetRadius(), dz.GetGroupCount()));
		}

		m_aZones     = m_aTempZones;
		m_aTempZones = null;
	}

	protected bool OnLocationFound(IEntity entity)
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

		int   groupCount;
		float radius;

		if (descType == TYPE_TOWN)
		{
			groupCount = m_iGroupsPerTown;
			radius     = RADIUS_TOWN;
		}
		else if (descType == TYPE_VILLAGE)
		{
			groupCount = m_iGroupsPerVillage;
			radius     = RADIUS_VILLAGE;
		}
		else
		{
			groupCount = m_iGroupsPerSettlement;
			radius     = RADIUS_SETTLEMENT;
		}

		vector pos = entity.GetOrigin();
		m_aTempZones.Insert(new INS_AmbientPatrolZoneState(pos, radius, groupCount, descType));

		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientPatrolManager] Zone '%1' (type %2) at %3, %4 group(s).",
				locName, descType, pos.ToString(), groupCount));

		return true;
	}

	// Proximity check
	protected void TickProximityCheck()
	{
		if (!Replication.IsServer())
			return;

		array<vector> playerPositions = {};
		array<int>    playerIds       = {};

		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (controlled)
				playerPositions.Insert(controlled.GetOrigin());
		}

		foreach (INS_AmbientPatrolZoneState state : m_aZones)
		{
			float nearestDist = INS_ZoneHelpers.NearestDist(state.m_vCenter, playerPositions, m_fDeactivationRadius + 1);

			// Only activate if there is at least one position in the zone the player is far enough
			// from to spawn into. Without this check, activation succeeds but all 16 spawn attempts
			// in FindSafeSpawnPosition fail, leaving the zone stuck active with no enemies.
			float spawnableRange = m_fMinSpawnDistFromPlayer - state.m_fRadius;

			if (!state.m_bActive && !m_bJIPPause && state.m_bReadyToRespawn
				&& nearestDist <= m_fActivationRadius && nearestDist >= spawnableRange)
				ActivateZone(state);
			else if (state.m_bActive && nearestDist > m_fDeactivationRadius)
				DeactivateZone(state);
			else if (state.m_bActive)
				CheckClearance(state);
		}
	}

	// Zone activate / deactivate
	protected void ActivateZone(INS_AmbientPatrolZoneState state)
	{
		state.m_bActive        = true;
		state.m_bEverHadLiving = false;

		int spawnIdx = 0;

		if (!state.m_aSavedPositions.IsEmpty())
		{
			// Restore from last known positions (persistence)
			foreach (vector savedPos : state.m_aSavedPositions)
			{
				float surfY = GetGame().GetWorld().GetSurfaceY(savedPos[0], savedPos[2]);
				if (surfY <= 0.5)
					continue; // skip if over water

				m_CallQueue.CallLater(SpawnGroupAt, spawnIdx * m_iSpawnStaggerMs, false, Vector(savedPos[0], surfY, savedPos[2]), state);
				spawnIdx++;
			}

			state.m_aSavedPositions.Clear();
		}
		else
		{
			for (int i = 0; i < state.m_iGroupCount; i++)
			{
				vector spawnPos = INS_ZoneHelpers.RandomPositionInRadius(state.m_vCenter, state.m_fRadius);
				m_CallQueue.CallLater(SpawnGroupAt, spawnIdx * m_iSpawnStaggerMs, false, spawnPos, state);
				spawnIdx++;
			}
		}

		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientPatrolManager] Zone activated at %1. %2 group(s) queued.",
				state.m_vCenter.ToString(), spawnIdx));
	}

	protected void DeactivateZone(INS_AmbientPatrolZoneState state)
	{
		state.m_bActive = false;
		state.m_aSavedPositions.Clear();

		foreach (INS_PatrolAgent agent : state.m_aAgents)
		{
			// Save position before deleting
			if (agent.m_GroupEntity)
				state.m_aSavedPositions.Insert(agent.m_GroupEntity.GetOrigin());

			agent.m_bActive = false;

			if (agent.m_CurrentWaypoint)
			{
				AIWaypoint wp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
				if (wp)
					agent.RemoveWaypoint(wp);

				SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
				agent.m_CurrentWaypoint = null;
			}

			if (agent.m_GroupEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(agent.m_GroupEntity);
		}

		state.m_aAgents.Clear();

		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientPatrolManager] Zone deactivated at %1. %2 position(s) saved.",
				state.m_vCenter.ToString(), state.m_aSavedPositions.Count()));
	}

	protected void CheckClearance(INS_AmbientPatrolZoneState state)
	{
		int living = 0;

		foreach (INS_PatrolAgent agent : state.m_aAgents)
		{
			if (!agent.m_Group)
				continue;

			array<AIAgent> agents = {};
			agent.m_Group.GetAgents(agents);
			foreach (AIAgent a : agents)
			{
				IEntity agentEnt = a.GetControlledEntity();
				if (!agentEnt)
					continue;
				DamageManagerComponent dmg = DamageManagerComponent.Cast(agentEnt.FindComponent(DamageManagerComponent));
				if (dmg && !dmg.IsDestroyed())
					living++;
			}
		}

		if (living > 0)
		{
			state.m_bEverHadLiving = true;
			return;
		}

		// Still waiting for the initial spawn to complete
		if (!state.m_bEverHadLiving)
			return;

		// All killed, clean up dead group entities and start the respawn timer
		foreach (INS_PatrolAgent agent : state.m_aAgents)
		{
			agent.m_bActive = false;

			if (agent.m_CurrentWaypoint)
			{
				AIWaypoint wp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
				if (wp)
					agent.RemoveWaypoint(wp);

				SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
				agent.m_CurrentWaypoint = null;
			}

			if (agent.m_GroupEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(agent.m_GroupEntity);
		}

		state.m_aAgents.Clear();
		state.m_aSavedPositions.Clear();
		state.m_bActive         = false;
		state.m_bEverHadLiving  = false;
		state.m_bReadyToRespawn = false;

		m_CallQueue.CallLater(MarkZoneRespawnable, (int)(m_fRespawnDelay * 1000), false, state);

		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientPatrolManager] Zone cleared at %1. Respawn in %2s.",
				state.m_vCenter.ToString(), m_fRespawnDelay));
	}

	protected void MarkZoneRespawnable(INS_AmbientPatrolZoneState state)
	{
		state.m_bReadyToRespawn = true;
		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientPatrolManager] Zone at %1 ready to respawn.", state.m_vCenter.ToString()));
	}

	// Group spawning and patrol loop
	protected void SpawnGroupAt(vector pos, INS_AmbientPatrolZoneState state)
	{
		// Zone may have deactivated while this was sitting in the queue
		if (!state.m_bActive)
			return;

		// Reject positions too close to any player; try a few alternatives before giving up
		pos = FindSafeSpawnPosition(pos, state);
		if (pos == vector.Zero)
		{
			if (m_bDebugLog)
				Print("[INS_AmbientPatrolManager] No safe spawn position found — skipping group.");

			// If no agents exist yet the whole activation failed. Reset so the zone can
			// re-activate once the player has moved far enough away.
			if (state.m_aAgents.IsEmpty())
			{
				state.m_bActive         = false;
				state.m_bReadyToRespawn = false;
				m_CallQueue.CallLater(MarkZoneRespawnable, 30000, false, state);
			}
			return;
		}

		ResourceName prefab = m_aGroupPrefabs[Math.RandomInt(0, m_aGroupPrefabs.Count())];
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_AmbientPatrolManager] ERROR: Cannot load group prefab: " + prefab);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode    = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]     = pos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			Print("[INS_AmbientPatrolManager] ERROR: SpawnEntityPrefab returned null.");
			return;
		}

		// Patrol positions regenerate on activation. A persisted group just stacks on top of the
		// fresh spawn after a crash.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
			persistence.StopTracking(ent, true);

		INS_PatrolAgent agent = new INS_PatrolAgent(ent, state.m_vCenter, state.m_fRadius);
		state.m_aAgents.Insert(agent);

		m_CallQueue.CallLater(InitGroup, 500, false, agent);
		m_CallQueue.CallLater(PatrolTick, 1500, false, agent);
	}

	protected void InitGroup(INS_PatrolAgent agent)
	{
		if (!agent.m_bActive || !agent.m_GroupEntity)
			return;

		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(agent.m_GroupEntity);
		if (aiGroup)
			aiGroup.SpawnAllImmediately();
	}

	protected void PatrolTick(INS_PatrolAgent agent)
	{
		if (!agent.m_bActive || !agent.m_GroupEntity)
			return;

		if (agent.m_CurrentWaypoint)
		{
			AIWaypoint oldWp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
			if (oldWp)
				agent.RemoveWaypoint(oldWp);

			SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
			agent.m_CurrentWaypoint = null;
		}

		vector wpPos = INS_ZoneHelpers.RandomPositionInRadius(agent.m_ZoneCenter, agent.m_ZoneRadius);
		IEntity wpEnt = SpawnWaypointAt(wpPos);

		if (wpEnt)
		{
			AIWaypoint wp = AIWaypoint.Cast(wpEnt);
			if (wp)
				agent.AddWaypoint(wp);

			agent.m_CurrentWaypoint = wpEnt;
		}

		m_CallQueue.CallLater(PatrolTick, (int)(m_fPatrolWaypointInterval * 1000), false, agent);
	}

	protected IEntity SpawnWaypointAt(vector pos)
	{
		Resource wpRes = Resource.Load(INS_PATROL_WAYPOINT_PREFAB);
		if (!wpRes || !wpRes.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode    = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]     = pos;

		return GetGame().SpawnEntityPrefab(wpRes, GetGame().GetWorld(), params);
	}

	// Returns a spawn position at least m_fMinSpawnDistFromPlayer from all players.
	// Tries the candidate first, then up to 16 random positions within the zone.
	// Returns vector.Zero if no safe position is found.
	protected vector FindSafeSpawnPosition(vector candidate, INS_AmbientPatrolZoneState state)
	{
		array<vector> playerPositions = {};
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (controlled)
				playerPositions.Insert(controlled.GetOrigin());
		}

		if (IsPositionSafe(candidate, playerPositions))
			return candidate;

		for (int i = 0; i < 16; i++)
		{
			vector attempt = INS_ZoneHelpers.RandomPositionInRadius(state.m_vCenter, state.m_fRadius);
			if (IsPositionSafe(attempt, playerPositions))
				return attempt;
		}

		return vector.Zero;
	}

	protected bool IsPositionSafe(vector pos, array<vector> playerPositions)
	{
		float threshSq = m_fMinSpawnDistFromPlayer * m_fMinSpawnDistFromPlayer;
		foreach (vector pPos : playerPositions)
		{
			if (vector.DistanceSq(pos, pPos) < threshSq)
				return false;
		}
		return true;
	}

	// Helpers
}
