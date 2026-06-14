/**
 * INS_GarrisonManager
 * by borked.gb
 *
 * Garrison AI manager. Spawns infantry and or vehicle groups at
 * manually placed INS_GarrisonZone locations. Cleared zones do not respawn;
 * these are permanent garrisons, not ambient filler.
 *
 * Patrol loop (per group):
 *   1. A patrol waypoint spawns at a random position within the zone radius.
 *   2. The group walks to it using patrol movement.
 *   3. After m_fPatrolWaypointInterval seconds the waypoint is replaced with a new one.
 *   4. When enemies are spotted the AI enters combat and resumes patrol after.
 *
 * Zone lifecycle:
 *   - Activates when any player enters m_fActivationRadius.
 *   - Deactivates when all players leave m_fDeactivationRadius.
 *   - On deactivation all groups and waypoints are deleted.
 *   - Cleared zones are saved to disk and restored on restart.
 *
 * Setup:
 *   1. Attach to the GameMode entity.
 *   2. Set m_aInfantryGroupPrefabs and optionally m_aVehicleGroupPrefabs.
 *   3. Place INS_GarrisonZone components on GenericEntities in the world.
 */

// Patrol waypoint prefab used by all group agents. Walk speed, auto completes on arrival.
const ResourceName INS_PATROL_WAYPOINT_PREFAB = "{22A875E30470BD4F}Prefabs/AI/Waypoints/AIWaypoint_Patrol.et";

// Save file for cleared zone state. Persists across server restarts; deleted on clean mission end.
const string INS_GARRISON_SAVE_DIR  = "$profile:INS_Saves";
const string INS_GARRISON_SAVE_FILE = "$profile:INS_Saves/garrison_cleared.dat";

// Per group patrol state. Tracks the one live waypoint and schedules the next one.
class INS_PatrolAgent
{
	IEntity              m_GroupEntity;    // infantry SCR_AIGroup entity or vehicle entity
	IEntity              m_CrewGroupEntity; // vehicle groups only, holds the crew's SCR_AIGroup
	SCR_AIGroup          m_Group;          // used for waypoint operations
	vector               m_ZoneCenter;
	float                m_ZoneRadius;
	bool                 m_bActive;        // set false to stop scheduling new waypoints
	IEntity              m_CurrentWaypoint;

	void INS_PatrolAgent(IEntity groupEnt, vector center, float radius)
	{
		m_GroupEntity     = groupEnt;
		m_CrewGroupEntity = null;
		m_Group           = SCR_AIGroup.Cast(groupEnt);
		m_ZoneCenter      = center;
		m_ZoneRadius      = radius;
		m_bActive         = true;
		m_CurrentWaypoint = null;
	}

	void AddWaypoint(AIWaypoint wp)
	{
		if (m_Group)
			m_Group.AddWaypoint(wp);
	}

	void RemoveWaypoint(AIWaypoint wp)
	{
		if (m_Group)
			m_Group.RemoveWaypoint(wp);
	}
}

// Per zone runtime state.
class INS_ZoneState
{
	INS_GarrisonZone              m_Zone;
	bool                        m_bActive;
	bool                        m_bCleared;        // permanently cleared, will not reactivate
	bool                        m_bEverHadLiving;  // guards against clearing before members spawn
	ref array<ref INS_PatrolAgent> m_aPatrolAgents;

	void INS_ZoneState(INS_GarrisonZone zone)
	{
		m_Zone            = zone;
		m_bActive         = false;
		m_bCleared        = false;
		m_bEverHadLiving  = false;
		m_aPatrolAgents   = new array<ref INS_PatrolAgent>();
	}
}

[ComponentEditorProps(category: "Insurgency", description: "Spawns AI at designer-placed garrison zones. Zones are permanently cleared once all enemies are killed.")]
class INS_GarrisonManagerClass : SCR_BaseGameModeComponentClass {}

class INS_GarrisonManager : SCR_BaseGameModeComponent
{
	[Attribute()]
	protected ref array<ResourceName> m_aInfantryGroupPrefabs;

	[Attribute()]
	protected ref array<ResourceName> m_aVehicleGroupPrefabs;

	[Attribute("600", UIWidgets.EditBox,
		"Distance (m) at which a zone becomes active.")]
	protected float m_fActivationRadius;

	[Attribute("800", UIWidgets.EditBox,
		"Distance (m) at which an active zone despawns.")]
	protected float m_fDeactivationRadius;

	[Attribute("5", UIWidgets.EditBox,
		"Seconds between proximity checks.")]
	protected float m_fCheckInterval;

	[Attribute("2", UIWidgets.EditBox,
		"Extra infantry groups in cache zones.")]
	protected int m_iCacheZoneExtraGroups;

	[Attribute("60", UIWidgets.EditBox,
		"Seconds between waypoint replacements per group.")]
	protected float m_fPatrolWaypointInterval;

	[Attribute("1000", UIWidgets.EditBox,
		"Milliseconds between each queued group spawn. Spread spawns out to avoid frame spikes.")]
	protected int m_iSpawnStaggerMs;

	[Attribute("50", UIWidgets.EditBox,
		"Players more than this height (m) above terrain are ignored for zone activation. Stops helicopters from triggering garrisons on flyovers.")]
	protected float m_fMaxActivationAltitude;

	[Attribute("80", UIWidgets.EditBox,
		"Minimum distance (m) between garrison vehicle spawn points. Keeps vehicles from clustering on the same road spot.")]
	protected float m_fVehicleSpawnSeparation;

	protected ref array<ref INS_ZoneState> m_aZoneStates = {};
	protected bool m_bJIPPause = false;
	protected ref ScriptCallQueue m_CallQueue = new ScriptCallQueue();

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (spawns, activations, deactivations).")]
	protected bool m_bDebugLog = false;

	protected bool m_bEnabled = true;

	// Public accessors
	bool IsEnabled()                        { return m_bEnabled; }
	int  GetCacheZoneExtraGroups()          { return m_iCacheZoneExtraGroups; }
	void SetCacheZoneExtraGroups(int count) { m_iCacheZoneExtraGroups = count; }

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
			foreach (INS_ZoneState state : m_aZoneStates)
			{
				if (state.m_bActive)
					DespawnZone(state);
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

		m_bEnabled              = params.m_bGarrisonsEnabled;
		m_iCacheZoneExtraGroups = params.m_iGarrisonCacheExtraGroups;
	}

	// Lifecycle
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();

		array<INS_GarrisonZone> zones = {};
		int count = INS_GarrisonZone.CollectZones(zones);

		foreach (INS_GarrisonZone z : zones)
		{
			// Filter stale null refs from previous play sessions
			if (z && z.GetOwner())
				m_aZoneStates.Insert(new INS_ZoneState(z));
		}

		LoadClearedZones();

		Print(string.Format("[INS_GarrisonManager] Started. %1 garrison zones registered.", count));

		GetGame().GetCallqueue().CallLater(TickCallQueue, 100, true);

		if (!m_bEnabled)
			return;

		m_CallQueue.CallLater(TickProximityCheck, (int)(m_fCheckInterval * 1000), true);
	}

	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().Remove(TickCallQueue);

		foreach (INS_ZoneState state : m_aZoneStates)
		{
			if (state.m_bActive)
				DespawnZone(state);
		}

		// Delete the save on a clean end so the next run starts fresh.
		// Crashes and restarts don't reach here, so cleared zones persist across them.
		if (FileIO.FileExists(INS_GARRISON_SAVE_FILE))
			FileIO.DeleteFile(INS_GARRISON_SAVE_FILE);
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

		Print("[INS_GarrisonManager] Last player left, despawning all garrison zones.");
		foreach (INS_ZoneState state : m_aZoneStates)
		{
			if (state.m_bActive)
				DespawnZone(state);
		}
	}

	// Proximity check. Fires every m_fCheckInterval seconds.
	protected void TickProximityCheck()
	{
		if (!Replication.IsServer())
			return;

		array<int>    playerIds       = {};
		array<vector> playerPositions = {};

		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int pid : playerIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (!controlled)
				continue;

			vector pos = controlled.GetOrigin();
			float groundY = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
			if (pos[1] - groundY <= m_fMaxActivationAltitude)
				playerPositions.Insert(pos);
		}

		foreach (INS_ZoneState state : m_aZoneStates)
		{
			if (!state.m_Zone)
				continue;

			// Permanently cleared zones never reactivate
			if (state.m_bCleared)
				continue;

			IEntity zoneOwner = state.m_Zone.GetOwner();
			if (!zoneOwner)
				continue;

			vector zonePos     = zoneOwner.GetOrigin();
			float  nearestDist = INS_ZoneHelpers.NearestDist(zonePos, playerPositions, m_fDeactivationRadius + 1);

			if (!state.m_bActive && !m_bJIPPause && nearestDist <= m_fActivationRadius)
				SpawnZone(state);
			else if (state.m_bActive && nearestDist > m_fDeactivationRadius)
				DespawnZone(state);
			else if (state.m_bActive)
				CheckZoneClearance(state);
		}
	}

	// Zone spawn / despawn
	protected void SpawnZone(INS_ZoneState state)
	{
		state.m_bActive = true;

		INS_GarrisonZone zone   = state.m_Zone;
		IEntity        owner  = zone.GetOwner();
		vector         center = owner.GetOrigin();
		float          radius = zone.GetRadius();

		int groupCount = zone.GetGroupCount();
		if (zone.IsCacheZone())
			groupCount += m_iCacheZoneExtraGroups;

		int spawnIdx = 0;

		// Some zones only spawn vehicles and have no infantry prefabs, so skip the infantry
		// loop when the list is empty.
		if (m_aInfantryGroupPrefabs && !m_aInfantryGroupPrefabs.IsEmpty())
		{
			for (int i = 0; i < groupCount; i++)
			{
				ResourceName prefab = m_aInfantryGroupPrefabs[Math.RandomInt(0, m_aInfantryGroupPrefabs.Count())];
				vector spawnPos = INS_ZoneHelpers.RandomPositionInRadius(center, radius);
				m_CallQueue.CallLater(SpawnGroupAt, spawnIdx * m_iSpawnStaggerMs, false, prefab, spawnPos, center, radius, state);
				spawnIdx++;
			}
		}

		if (m_aVehicleGroupPrefabs && !m_aVehicleGroupPrefabs.IsEmpty())
		{
			int vehicleCount = zone.GetVehicleCount();
			if (vehicleCount > 0)
			{
				array<vector> vehiclePositions = {};
				FindDistinctRoadPoints(center, radius, vehicleCount, m_fVehicleSpawnSeparation, vehiclePositions);

				if (vehiclePositions.IsEmpty())
				{
					if (m_bDebugLog)
						Print("[INS_GarrisonManager] No road found in zone — vehicle spawn skipped.");
				}
				else
				{
					if (m_bDebugLog && vehiclePositions.Count() < vehicleCount)
						Print(string.Format("[INS_GarrisonManager] Only %1 of %2 vehicle position(s) found with required separation.", vehiclePositions.Count(), vehicleCount));

					for (int v = 0; v < vehiclePositions.Count(); v++)
					{
						ResourceName prefab = m_aVehicleGroupPrefabs[Math.RandomInt(0, m_aVehicleGroupPrefabs.Count())];
						m_CallQueue.CallLater(SpawnVehicleAt, spawnIdx * m_iSpawnStaggerMs, false, prefab, vehiclePositions[v], center, radius, state);
						spawnIdx++;
					}
				}
			}
		}

		if (m_bDebugLog)
			Print(string.Format("[INS_GarrisonManager] Zone activated at %1. %2 group(s) queued.",
				center.ToString(), spawnIdx));
	}

	protected void DespawnZone(INS_ZoneState state)
	{
		state.m_bActive = false;

		foreach (INS_PatrolAgent agent : state.m_aPatrolAgents)
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

			bool deleteVehicle = true;

			if (agent.m_GroupEntity)
			{
				// For vehicle agents, check for player occupants before deleting anything
				if (agent.m_CrewGroupEntity)
				{
					SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
						agent.m_GroupEntity.FindComponent(SCR_BaseCompartmentManagerComponent));
					if (compMgr)
					{
						array<IEntity> occupants = {};
						compMgr.GetOccupants(occupants);

						foreach (IEntity occ : occupants)
						{
							if (!occ)
								continue;
							if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(occ) > 0)
							{
								deleteVehicle = false;
								break;
							}
						}

						if (deleteVehicle)
						{
							foreach (IEntity occ : occupants)
							{
								if (occ)
									SCR_EntityHelper.DeleteEntityAndChildren(occ);
							}
						}
					}
				}

				if (deleteVehicle)
					SCR_EntityHelper.DeleteEntityAndChildren(agent.m_GroupEntity);
			}

			// The crew group belongs to the vehicle. If a player has taken it, leave the crew
			// alone rather than deleting their group out from under them.
			if (deleteVehicle && agent.m_CrewGroupEntity)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CrewGroupEntity);
				agent.m_CrewGroupEntity = null;
			}
		}

		state.m_aPatrolAgents.Clear();

		if (m_bDebugLog)
			Print("[INS_GarrisonManager] Zone deactivated.");
	}

	// Spawn a single group and start its patrol loop.
	protected void SpawnGroupAt(ResourceName prefab, vector pos,
		vector zoneCenter, float zoneRadius, INS_ZoneState state)
	{
		if (!state.m_bActive)
			return;

		if (prefab.IsEmpty())
			return;

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_GarrisonManager] ERROR: Cannot load group prefab: " + prefab);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode     = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]      = pos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			Print("[INS_GarrisonManager] ERROR: SpawnEntityPrefab returned null for: " + prefab);
			return;
		}

		// Garrison state is in our .dat files. Without this, a crash reload respawns the entity
		// and then the garrison script runs again, giving you two full garrisons.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
			persistence.StopTracking(ent, true);

		if (m_bDebugLog)
			Print("[INS_GarrisonManager] Entity spawned OK at " + pos.ToString());

		INS_PatrolAgent agent = new INS_PatrolAgent(ent, zoneCenter, zoneRadius);
		state.m_aPatrolAgents.Insert(agent);

		m_CallQueue.CallLater(InitGroup, 500, false, agent);
		m_CallQueue.CallLater(PatrolTick, 1500, false, agent);
	}

	protected void InitGroup(INS_PatrolAgent agent)
	{
		if (!agent.m_bActive || !agent.m_GroupEntity)
			return;

		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(agent.m_GroupEntity);
		if (aiGroup)
		{
			if (m_bDebugLog)
				Print("[INS_GarrisonManager] PRE-spawn members queued: " + aiGroup.GetNumberOfMembersToSpawn());
			aiGroup.SpawnAllImmediately();
			if (m_bDebugLog)
				Print("[INS_GarrisonManager] POST-spawn members queued: " + aiGroup.GetNumberOfMembersToSpawn());
		}
		else
		{
			if (m_bDebugLog)
				Print("[INS_GarrisonManager] NOTE: entity is not SCR_AIGroup (vehicle?) — skipping SpawnAllImmediately");
		}
	}

	protected void PatrolTick(INS_PatrolAgent agent)
	{
		if (!agent.m_bActive)
			return;

		if (!agent.m_GroupEntity)
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

		int intervalMs = (int)(m_fPatrolWaypointInterval * 1000);
		m_CallQueue.CallLater(PatrolTick, intervalMs, false, agent);
	}

	protected void SpawnVehicleAt(ResourceName prefab, vector pos,
		vector zoneCenter, float zoneRadius, INS_ZoneState state)
	{
		if (!state.m_bActive)
			return;

		if (prefab.IsEmpty())
			return;

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_GarrisonManager] ERROR: Cannot load vehicle prefab: " + prefab);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode     = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]      = pos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			Print("[INS_GarrisonManager] ERROR: SpawnEntityPrefab returned null for vehicle: " + prefab);
			return;
		}

		// Same as SpawnGroupAt. If the vehicle gets saved, a crash reload brings it back and the
		// garrison script spawns another on top.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
			persistence.StopTracking(ent, true);

		if (m_bDebugLog)
			Print("[INS_GarrisonManager] Vehicle spawned on road at " + pos.ToString());

		INS_PatrolAgent agent = new INS_PatrolAgent(ent, zoneCenter, zoneRadius);
		state.m_aPatrolAgents.Insert(agent);

		m_CallQueue.CallLater(SpawnVehicleCrew, 500, false, agent);
		m_CallQueue.CallLater(InitVehicleGroup, 1000, false, agent);
	}

	protected void SpawnVehicleCrew(INS_PatrolAgent agent)
	{
		if (!agent.m_bActive || !agent.m_GroupEntity)
			return;

		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(
			agent.m_GroupEntity.FindComponent(SCR_DamageManagerComponent));
		if (dmg && dmg.GetHealthScaled() <= 0)
		{
			if (m_bDebugLog)
				Print("[INS_GarrisonManager] Vehicle was destroyed before crew could spawn. Skipping.");
			return;
		}

		vector mat[4];
		agent.m_GroupEntity.GetTransform(mat);
		if (mat[1][1] < 0.7)
		{
			if (m_bDebugLog)
				Print("[INS_GarrisonManager] Vehicle tipped over before crew could spawn. Skipping.");
			return;
		}

		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
			agent.m_GroupEntity.FindComponent(SCR_BaseCompartmentManagerComponent));

		if (!compMgr)
		{
			Print("[INS_GarrisonManager] ERROR: No SCR_BaseCompartmentManagerComponent on vehicle.");
			return;
		}

		bool ok = compMgr.SpawnDefaultOccupants(SCR_BaseCompartmentManagerComponent.CREW_COMPARTMENT_TYPES);
		if (m_bDebugLog)
			Print("[INS_GarrisonManager] SpawnDefaultOccupants result: " + ok);
	}

	protected void InitVehicleGroup(INS_PatrolAgent agent)
	{
		if (!agent.m_bActive || !agent.m_GroupEntity)
			return;

		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
			agent.m_GroupEntity.FindComponent(SCR_BaseCompartmentManagerComponent));

		if (!compMgr)
			return;

		array<IEntity> occupants = {};
		compMgr.GetOccupants(occupants);
		if (m_bDebugLog)
			Print("[INS_GarrisonManager] Vehicle occupant count: " + occupants.Count());

		PersistenceSystem persistence = PersistenceSystem.GetInstance();

		foreach (IEntity occ : occupants)
		{
			// Crew members need untracking too, for the same reason as the vehicle.
			if (persistence)
				persistence.StopTracking(occ, true);

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(occ);
			if (!character)
				continue;

			AIControlComponent ctrl = character.GetAIControlComponent();
			if (!ctrl)
				continue;

			AIAgent ag = ctrl.GetAIAgent();
			if (!ag)
				continue;

			AIGroup grp = ag.GetParentGroup();
			SCR_AIGroup scrGroup = SCR_AIGroup.Cast(grp);
			if (scrGroup)
			{
				if (persistence)
					persistence.StopTracking(scrGroup, true);

				agent.m_Group = scrGroup;
				agent.m_CrewGroupEntity = scrGroup;
				if (m_bDebugLog)
					Print("[INS_GarrisonManager] Vehicle crew group acquired. Crew will react to threats.");
				m_CallQueue.CallLater(PatrolTick, 500, false, agent);
				return;
			}
		}

		if (m_bDebugLog)
			Print("[INS_GarrisonManager] WARNING: Could not acquire vehicle crew group.");
	}

	protected void CheckZoneClearance(INS_ZoneState state)
	{
		int living = 0;
		array<AIAgent> agents = {};

		foreach (INS_PatrolAgent agent : state.m_aPatrolAgents)
		{
			if (!agent.m_Group)
				continue;

			agents.Clear();
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

		if (!state.m_bEverHadLiving)
			return;

		state.m_bCleared = true;
		DespawnZone(state);
		SaveClearedZones();

		IEntity zoneOwner = state.m_Zone.GetOwner();
		string posStr = "unknown";
		if (zoneOwner)
			posStr = zoneOwner.GetOrigin().ToString();

		Print("[INS_GarrisonManager] Zone CLEARED at " + posStr + ". Will not respawn.");
	}

	// Persistence
	protected void SaveClearedZones()
	{
		FileIO.MakeDirectory(INS_GARRISON_SAVE_DIR);

		FileSerializer file = new FileSerializer();
		if (!file.Open(INS_GARRISON_SAVE_FILE, FileMode.WRITE))
		{
			Print("[INS_GarrisonManager] WARNING: Could not open save file for writing.");
			return;
		}

		int count = 0;
		foreach (INS_ZoneState state : m_aZoneStates)
		{
			if (state.m_bCleared && state.m_Zone && state.m_Zone.GetOwner())
				count++;
		}

		file.Write(count);

		foreach (INS_ZoneState state : m_aZoneStates)
		{
			if (!state.m_bCleared || !state.m_Zone || !state.m_Zone.GetOwner())
				continue;

			vector pos = state.m_Zone.GetOwner().GetOrigin();
			float x = pos[0];
			float z = pos[2];
			file.Write(x);
			file.Write(z);
		}

		file.Close();
		Print(string.Format("[INS_GarrisonManager] Saved %1 cleared zone(s).", count));
	}

	protected void LoadClearedZones()
	{
		if (!FileIO.FileExists(INS_GARRISON_SAVE_FILE))
			return;

		FileSerializer file = new FileSerializer();
		if (!file.Open(INS_GARRISON_SAVE_FILE, FileMode.READ))
			return;

		int count;
		file.Read(count);

		array<vector> clearedPositions = {};
		for (int i = 0; i < count; i++)
		{
			float x, z;
			file.Read(x);
			file.Read(z);
			clearedPositions.Insert(Vector(x, 0, z));
		}

		file.Close();

		int marked = 0;
		foreach (INS_ZoneState state : m_aZoneStates)
		{
			if (!state.m_Zone || !state.m_Zone.GetOwner())
				continue;

			vector zonePos = state.m_Zone.GetOwner().GetOrigin();
			zonePos[1] = 0;

			foreach (vector saved : clearedPositions)
			{
				if (vector.DistanceSq(zonePos, saved) < 25.0)
				{
					state.m_bCleared = true;
					marked++;
					break;
				}
			}
		}

		Print(string.Format("[INS_GarrisonManager] Loaded save: %1/%2 zone(s) pre-cleared.", marked, count));
	}

	// Helpers
	protected IEntity SpawnWaypointAt(vector pos)
	{
		Resource wpRes = Resource.Load(INS_PATROL_WAYPOINT_PREFAB);
		if (!wpRes || !wpRes.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode     = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]      = pos;

		return GetGame().SpawnEntityPrefab(wpRes, GetGame().GetWorld(), params);
	}

	// Collects up to 'count' road vertices near 'center' within 'radius', each at least
	// 'minSeparation' metres apart. Shuffles candidates first so the selection is random.
	// Falls back to less-separated points if not enough are found at the required distance.
	protected void FindDistinctRoadPoints(vector center, float radius, int count,
		float minSeparation, out array<vector> outPoints)
	{
		outPoints = new array<vector>();

		AIWorld aiWorldBase = GetGame().GetAIWorld();
		if (!aiWorldBase)
			return;

		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(aiWorldBase);
		if (!aiWorld)
			return;

		RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
		if (!roadMgr)
			return;

		BaseRoad foundRoad;
		float roadDist;
		roadMgr.GetClosestRoad(center, foundRoad, roadDist);

		if (!foundRoad || roadDist > radius)
			return;

		array<vector> roadPoints = new array<vector>();
		foundRoad.GetPoints(roadPoints);

		if (roadPoints.IsEmpty())
			return;

		// Collect vertices within the zone radius
		float radiusSq = radius * radius;
		array<vector> candidates = new array<vector>();
		foreach (vector rp : roadPoints)
		{
			if (vector.DistanceSq(rp, center) <= radiusSq)
				candidates.Insert(rp);
		}

		// If no vertices inside the zone, use the single closest one
		if (candidates.IsEmpty())
		{
			vector best = roadPoints[0];
			float bestDistSq = float.MAX;
			foreach (vector rp : roadPoints)
			{
				float dSq = vector.DistanceSq(rp, center);
				if (dSq < bestDistSq)
				{
					bestDistSq = dSq;
					best = rp;
				}
			}
			best[1] = GetGame().GetWorld().GetSurfaceY(best[0], best[2]);
			outPoints.Insert(best);
			return;
		}

		// Shuffle so each spawn picks a different random section of road
		for (int i = candidates.Count() - 1; i > 0; i--)
		{
			int j = Math.RandomInt(0, i + 1);
			vector tmp = candidates[i];
			candidates.Set(i, candidates[j]);
			candidates.Set(j, tmp);
		}

		// First pass: pick with minimum separation enforced
		foreach (vector candidate : candidates)
		{
			if (outPoints.Count() >= count)
				break;

			bool tooClose = false;
			float sepSq = minSeparation * minSeparation;
			foreach (vector picked : outPoints)
			{
				if (vector.DistanceSq(candidate, picked) < sepSq)
				{
					tooClose = true;
					break;
				}
			}

			if (!tooClose)
			{
				vector pt = candidate;
				pt[1] = GetGame().GetWorld().GetSurfaceY(pt[0], pt[2]);
				outPoints.Insert(pt);
			}
		}

		// Second pass: relax separation if we still need more points
		if (outPoints.Count() < count)
		{
			foreach (vector candidate : candidates)
			{
				if (outPoints.Count() >= count)
					break;

				bool already = false;
				foreach (vector picked : outPoints)
				{
					if (candidate == picked)
					{
						already = true;
						break;
					}
				}

				if (!already)
				{
					vector pt = candidate;
					pt[1] = GetGame().GetWorld().GetSurfaceY(pt[0], pt[2]);
					outPoints.Insert(pt);
				}
			}
		}
	}

}
