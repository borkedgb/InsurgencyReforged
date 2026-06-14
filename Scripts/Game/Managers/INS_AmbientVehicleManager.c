/**
 * INS_AmbientVehicleManager
 * by borked.gb
 *
 * Spawns enemy vehicles that drive between towns, villages, and settlements along roads.
 * Routes are built automatically by snapping map locations to the road network.
 *
 * Vehicle lifecycle per route:
 *   1. A route endpoint near a player is chosen as spawn point (furthest from player,
 *      but still within activation radius).
 *   2. Vehicle spawns oriented toward the opposite endpoint.
 *   3. Crew compartments are filled (200ms deferred) and their AI group is resolved (1000ms).
 *   4. A Move waypoint is assigned at the destination (1500ms).
 *   5. The crew drives there, engaging any hostiles en route.
 *   6. On arrival: if no player nearby, despawn. Otherwise wait m_fRerouteDelay then
 *      send them back the other way.
 *   7. If stuck for m_fStuckTimeout seconds with no player watching, despawn.
 *   8. If crew are all killed, the vehicle stays as a wreck and is removed from tracking.
 *
 * Designer overrides:
 *   Place INS_AmbientZone components with m_bIsVehicleNode set to add extra route nodes
 *
 * Setup:
 *   Attach to GameMode entity. Add one or more enemy vehicle prefabs to m_aVehiclePrefabs
 *   (each must have default crew compartments pre-configured). One is picked at random per spawn.
 */

// Move waypoint prefab, used to send the crew to their destination.
const ResourceName INS_MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";

// A bidirectional road-snapped route between two locations.
class INS_AmbientVehicleRoute
{
	vector m_vStart;      // location centre (start)
	vector m_vEnd;        // location centre (end)
	string m_sStartName;
	string m_sEndName;
	vector m_vRoadStart;  // road-snapped spawn/destination point at start
	vector m_vRoadEnd;    // road-snapped spawn/destination point at end

	void INS_AmbientVehicleRoute(vector start, vector end,
		string startName, string endName, vector roadStart, vector roadEnd)
	{
		m_vStart     = start;
		m_vEnd       = end;
		m_sStartName = startName;
		m_sEndName   = endName;
		m_vRoadStart = roadStart;
		m_vRoadEnd   = roadEnd;
	}
}

// Per-vehicle runtime state.
class INS_AmbientVehicleAgent
{
	IEntity     m_VehicleEntity;
	SCR_AIGroup m_CrewGroup;
	IEntity     m_CurrentWaypoint;
	bool        m_bActive;
	bool        m_bEverHadCrew;    // guards against treating pre-spawn state as "all killed"
	bool        m_bForward;        // true = travelling start to end, false = end to start
	vector      m_vDestination;
	vector      m_vLastPosition;
	float       m_fStuckTime;
	float       m_fArrivalWaitTime;
	float       m_fPlayerGraceRemaining; // seconds left before despawn is allowed after a player last exited
	int         m_iRouteIndex;

	void INS_AmbientVehicleAgent(IEntity vehicle, vector dest, int routeIdx, bool forward)
	{
		m_VehicleEntity         = vehicle;
		m_CrewGroup             = null;
		m_CurrentWaypoint       = null;
		m_bActive               = true;
		m_bEverHadCrew          = false;
		m_bForward              = forward;
		m_vDestination          = dest;
		m_vLastPosition         = vehicle.GetOrigin();
		m_fStuckTime            = 0;
		m_fArrivalWaitTime      = 0;
		m_fPlayerGraceRemaining = 0;
		m_iRouteIndex           = routeIdx;
	}
}

[ComponentEditorProps(category: "Insurgency", description: "Ambient enemy vehicles patrolling routes between towns. Attach to GameMode entity.")]
class INS_AmbientVehicleManagerClass : SCR_BaseGameModeComponentClass {}

class INS_AmbientVehicleManager : SCR_BaseGameModeComponent
{
	[Attribute()]
	protected ref array<ResourceName> m_aVehiclePrefabs;

	[Attribute("4", UIWidgets.EditBox, "Maximum vehicles active at the same time.")]
	protected int m_iMaxActiveVehicles;

	[Attribute("800", UIWidgets.EditBox, "Distance (m) from a route endpoint at which that route becomes a spawn candidate.")]
	protected float m_fActivationRadius;

	[Attribute("1000", UIWidgets.EditBox, "Distance (m) at which an active vehicle despawns.")]
	protected float m_fDeactivationRadius;

	[Attribute("500", UIWidgets.EditBox, "Minimum distance from any player to spawn — prevents pop-in.")]
	protected float m_fMinSpawnDistance;

[Attribute("3000", UIWidgets.EditBox, "Maximum straight-line distance between two zones for a valid route.")]
	protected float m_fMaxRouteDistance;

	[Attribute("500", UIWidgets.EditBox, "Maximum distance from zone centre to nearest road. Zones further from roads are skipped.")]
	protected float m_fMaxRoadSnapDistance;

	[Attribute("75", UIWidgets.EditBox, "Distance (m) from destination at which vehicle is considered arrived.")]
	protected float m_fArrivalDistance;

	[Attribute("30", UIWidgets.EditBox, "Seconds vehicle waits at destination before rerouting (when a player is nearby).")]
	protected float m_fRerouteDelay;

	[Attribute("30", UIWidgets.EditBox, "Seconds without movement before vehicle is considered stuck and despawned.")]
	protected float m_fStuckTimeout;

	[Attribute("5", UIWidgets.EditBox, "Seconds between proximity checks.")]
	protected float m_fCheckInterval;

	[Attribute("500", UIWidgets.EditBox, "Milliseconds between each queued vehicle spawn. Spread spawns out to avoid frame spikes.")]
	protected int m_iSpawnStaggerMs;

	[Attribute("1800", UIWidgets.EditBox, "Seconds before a vehicle wreck is removed by the garbage system (0 = never).")]
	protected float m_fWreckLifetime;

	protected static const int TYPE_TOWN       = 59;
	protected static const int TYPE_VILLAGE    = 60;
	protected static const int TYPE_SETTLEMENT = 61;

	protected bool m_bEnabled = true;
	protected bool m_bJIPPause = false;
	protected ref ScriptCallQueue m_CallQueue = new ScriptCallQueue();

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (spawns, routes, crew).")]
	protected bool m_bDebugLog = false;

	protected ref array<ref INS_AmbientVehicleRoute> m_aRoutes         = {};
	protected ref array<ref INS_AmbientVehicleAgent> m_aActiveVehicles = {};

	// Temp storage used only during route building, freed immediately after
	protected ref array<vector> m_aTempZonePositions;
	protected ref array<string> m_aTempZoneNames;

	// Public accessors.
	bool IsEnabled()              { return m_bEnabled; }
	int  GetMaxActiveVehicles()   { return m_iMaxActiveVehicles; }
	void SetMaxActiveVehicles(int count) { m_iMaxActiveVehicles = count; }

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
			for (int i = m_aActiveVehicles.Count() - 1; i >= 0; i--)
				DespawnVehicle(m_aActiveVehicles[i]);
		}
		else
		{
			if (!m_aRoutes.IsEmpty())
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

		m_bEnabled           = params.m_bVehiclesEnabled;
		m_iMaxActiveVehicles = params.m_iMaxActiveVehicles;
	}

	// Lifecycle.
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();

		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
		{
			Print("[INS_AmbientVehicleManager] WARNING: No vehicle prefabs set — disabled.");
			return;
		}

		DiscoverZonesAndBuildRoutes();

		Print(string.Format("[INS_AmbientVehicleManager] Started — %1 routes built.", m_aRoutes.Count()));

		if (!m_aRoutes.IsEmpty())
		{
			GetGame().GetCallqueue().CallLater(TickCallQueue, 100, true);
			if (m_bEnabled)
				m_CallQueue.CallLater(TickProximityCheck, (int)(m_fCheckInterval * 1000), true);
		}
	}

	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().Remove(TickCallQueue);

		for (int i = m_aActiveVehicles.Count() - 1; i >= 0; i--)
			DespawnVehicle(m_aActiveVehicles[i]);
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

		Print("[INS_AmbientVehicleManager] Last player left, despawning all vehicles.");
		for (int i = m_aActiveVehicles.Count() - 1; i >= 0; i--)
			DespawnVehicle(m_aActiveVehicles[i]);
	}

	// Zone discovery and route building.
	protected void DiscoverZonesAndBuildRoutes()
	{
		m_aTempZonePositions = new array<vector>();
		m_aTempZoneNames     = new array<string>();

		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		GetGame().GetWorld().QueryEntitiesByAABB(mins, maxs, OnZoneFound);

		// Add INS_AmbientZone vehicle nodes that auto-discovery won't find
		array<INS_AmbientZone> designerZones = {};
		INS_AmbientZone.CollectZones(designerZones);

		foreach (INS_AmbientZone dz : designerZones)
		{
			if (!dz.IsVehicleNode())
				continue;

			IEntity dzOwner = dz.GetOwner();
			if (!dzOwner)
				continue;

			vector dzPos = dzOwner.GetOrigin();

			// Skip if too close to an already discovered zone
			bool duplicate = false;
			foreach (vector existing : m_aTempZonePositions)
			{
				vector diff = dzPos - existing;
				if (diff.Length() <= 100)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
			{
				m_aTempZonePositions.Insert(dzPos);
				m_aTempZoneNames.Insert("Custom");
				if (m_bDebugLog)
					Print(string.Format("[INS_AmbientVehicleManager] Added designer vehicle node at %1.", dzPos.ToString()));
			}
		}

		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientVehicleManager] %1 candidate zone(s) found.", m_aTempZonePositions.Count()));

		BuildRoutes();

		m_aTempZonePositions = null;
		m_aTempZoneNames     = null;
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

		m_aTempZonePositions.Insert(entity.GetOrigin());
		m_aTempZoneNames.Insert(locName);

		return true;
	}

	protected void BuildRoutes()
	{
		AIWorld aiWorldBase = GetGame().GetAIWorld();
		if (!aiWorldBase)
		{
			Print("[INS_AmbientVehicleManager] ERROR: No AIWorld — cannot build routes.");
			return;
		}

		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(aiWorldBase);
		if (!aiWorld)
		{
			Print("[INS_AmbientVehicleManager] ERROR: AIWorld cast to ChimeraAIWorld failed.");
			return;
		}

		RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
		if (!roadMgr)
		{
			Print("[INS_AmbientVehicleManager] WARNING: No RoadNetworkManager — vehicles disabled.");
			return;
		}

		int zoneCount = m_aTempZonePositions.Count();
		ref array<vector> roadPositions = new array<vector>();
		ref array<bool>   hasRoad       = new array<bool>();

		// Snap each zone to its nearest road vertex
		for (int i = 0; i < zoneCount; i++)
		{
			vector zonePos = m_aTempZonePositions[i];
			BaseRoad foundRoad;
			float    roadDist;

			roadMgr.GetClosestRoad(zonePos, foundRoad, roadDist);

			if (!foundRoad || roadDist > m_fMaxRoadSnapDistance)
			{
				roadPositions.Insert(vector.Zero);
				hasRoad.Insert(false);
				if (m_bDebugLog)
					Print(string.Format("[INS_AmbientVehicleManager] '%1' has no road within %2m — skipped.",
						m_aTempZoneNames[i], m_fMaxRoadSnapDistance));
				continue;
			}

			// Walk the road's vertex list to find the closest point
			ref array<vector> roadPoints = new array<vector>();
			foundRoad.GetPoints(roadPoints);

			if (roadPoints.IsEmpty())
			{
				roadPositions.Insert(vector.Zero);
				hasRoad.Insert(false);
				if (m_bDebugLog)
					Print(string.Format("[INS_AmbientVehicleManager] '%1' road has no vertex data — skipped.", m_aTempZoneNames[i]));
				continue;
			}

			vector bestPoint = zonePos;
			float  bestDist  = 999999;

			foreach (vector rp : roadPoints)
			{
				vector diff = rp - zonePos;
				float d = diff.Length();
				if (d < bestDist)
				{
					bestDist  = d;
					bestPoint = rp;
				}
			}

			// Reject if the nearest vertex is too far — the road passes near the zone but has no
			// vertex close enough to give a sensible spawn point.
			if (bestDist > m_fMaxRoadSnapDistance)
			{
				roadPositions.Insert(vector.Zero);
				hasRoad.Insert(false);
				if (m_bDebugLog)
					Print(string.Format("[INS_AmbientVehicleManager] '%1' closest road vertex at %2m — too far, skipped.",
						m_aTempZoneNames[i], (int)bestDist));
				continue;
			}

			// Sample terrain height at the road point
			bestPoint[1] = GetGame().GetWorld().GetSurfaceY(bestPoint[0], bestPoint[2]);

			roadPositions.Insert(bestPoint);
			hasRoad.Insert(true);

			if (m_bDebugLog)
				Print(string.Format("[INS_AmbientVehicleManager] '%1' snapped to road at %2 (%3m away).",
					m_aTempZoneNames[i], bestPoint.ToString(), (int)roadDist));
		}

		// Build routes between all zone pairs within range
		for (int a = 0; a < zoneCount; a++)
		{
			if (!hasRoad[a])
				continue;

			for (int b = a + 1; b < zoneCount; b++)
			{
				if (!hasRoad[b])
					continue;

				vector diff = m_aTempZonePositions[a] - m_aTempZonePositions[b];
				if (diff.Length() > m_fMaxRouteDistance)
					continue;

				m_aRoutes.Insert(new INS_AmbientVehicleRoute(
					m_aTempZonePositions[a], m_aTempZonePositions[b],
					m_aTempZoneNames[a],     m_aTempZoneNames[b],
					roadPositions[a],        roadPositions[b]
				));

				if (m_bDebugLog)
					Print(string.Format("[INS_AmbientVehicleManager] Route: '%1' <-> '%2'",
						m_aTempZoneNames[a], m_aTempZoneNames[b]));
			}
		}
	}

	// Proximity tick, manages existing vehicles and queues new spawns.
	protected void TickProximityCheck()
	{
		if (!Replication.IsServer())
			return;

		array<vector> playerPos = {};
		array<int>    playerIds = {};

		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (controlled)
				playerPos.Insert(controlled.GetOrigin());
		}

		if (playerPos.IsEmpty())
			return;

		// Manage existing vehicles (iterate backward so removal is safe)
		for (int i = m_aActiveVehicles.Count() - 1; i >= 0; i--)
		{
			INS_AmbientVehicleAgent agent = m_aActiveVehicles[i];

			if (!agent.m_bActive || !agent.m_VehicleEntity)
			{
				m_aActiveVehicles.Remove(i);
				continue;
			}

			// Crew all killed. Leave as a wreck and stop tracking.
			if (agent.m_bEverHadCrew && agent.m_CrewGroup)
			{
				array<AIAgent> crewAgents = {};
				agent.m_CrewGroup.GetAgents(crewAgents);
				int crewAlive = 0;
				foreach (AIAgent a : crewAgents)
				{
					IEntity agentEnt = a.GetControlledEntity();
					if (!agentEnt)
						continue;
					DamageManagerComponent dmg = DamageManagerComponent.Cast(agentEnt.FindComponent(DamageManagerComponent));
					if (dmg && !dmg.IsDestroyed())
						crewAlive++;
				}
				if (crewAlive == 0)
				{
					if (m_bDebugLog)
					Print("[INS_AmbientVehicleManager] Vehicle crew killed. Leaving wreck, removing from tracking.");
					agent.m_bActive = false;

					if (agent.m_CurrentWaypoint)
					{
						AIWaypoint wp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
						if (wp)
							agent.m_CrewGroup.RemoveWaypoint(wp);

						SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
						agent.m_CurrentWaypoint = null;
					}

					if (!HasPlayerOccupant(agent.m_VehicleEntity) && m_fWreckLifetime > 0 && agent.m_VehicleEntity)
					{
						GarbageSystem gs = SCR_GarbageSystem.GetByEntityWorld(agent.m_VehicleEntity);
						if (gs)
							gs.Insert(agent.m_VehicleEntity, m_fWreckLifetime);
					}

					m_aActiveVehicles.Remove(i);
					continue;
				}
			}

			vector vehPos       = agent.m_VehicleEntity.GetOrigin();
			float  nearestPlayer = INS_ZoneHelpers.NearestDist(vehPos, playerPos, m_fDeactivationRadius + 1);

			// Keep a grace window open for 5 minutes after a player last occupied the vehicle
			if (HasPlayerOccupant(agent.m_VehicleEntity))
				agent.m_fPlayerGraceRemaining = 300;
			else if (agent.m_fPlayerGraceRemaining > 0)
				agent.m_fPlayerGraceRemaining -= m_fCheckInterval;

			if (nearestPlayer > m_fDeactivationRadius && agent.m_fPlayerGraceRemaining <= 0)
			{
				DespawnVehicle(agent);
				continue;
			}

			// Arrival check
			vector destDiff = vehPos - agent.m_vDestination;
			if (destDiff.Length() <= m_fArrivalDistance)
			{
				agent.m_fArrivalWaitTime += m_fCheckInterval;
				if (agent.m_fArrivalWaitTime >= m_fRerouteDelay)
				{
					RerouteVehicle(agent);
					agent.m_fArrivalWaitTime = 0;
				}
			}

			// Stuck check
			vector moveDiff = vehPos - agent.m_vLastPosition;
			if (moveDiff.Length() < 2.0)
			{
				agent.m_fStuckTime += m_fCheckInterval;
				if (agent.m_fStuckTime >= m_fStuckTimeout && nearestPlayer > m_fDeactivationRadius && agent.m_fPlayerGraceRemaining <= 0)
				{
					if (m_bDebugLog)
					Print("[INS_AmbientVehicleManager] Vehicle stuck. Despawning.");
					DespawnVehicle(agent);
					continue;
				}
			}
			else
			{
				agent.m_fStuckTime = 0;
			}

			agent.m_vLastPosition = vehPos;
		}

		// Try to spawn a vehicle on a new route if under the cap
		if (m_aActiveVehicles.Count() >= m_iMaxActiveVehicles)
			return;

		if (m_bJIPPause)
			return;

		array<int> candidates = {};
		for (int r = 0; r < m_aRoutes.Count(); r++)
		{
			// One vehicle per route at a time
			bool inUse = false;
			foreach (INS_AmbientVehicleAgent active : m_aActiveVehicles)
			{
				if (active.m_iRouteIndex == r)
				{
					inUse = true;
					break;
				}
			}
			if (inUse)
				continue;

			INS_AmbientVehicleRoute route = m_aRoutes[r];
			float distToStart = INS_ZoneHelpers.NearestDist(route.m_vStart, playerPos, m_fDeactivationRadius + 1);
			float distToEnd   = INS_ZoneHelpers.NearestDist(route.m_vEnd, playerPos, m_fDeactivationRadius + 1);

			if (distToStart <= m_fActivationRadius || distToEnd <= m_fActivationRadius)
				candidates.Insert(r);
		}

		if (candidates.IsEmpty())
			return;

		int slots    = m_iMaxActiveVehicles - m_aActiveVehicles.Count();
		int toSpawn  = Math.Min(slots, candidates.Count());

		for (int s = 0; s < toSpawn; s++)
		{
			int pick         = Math.RandomInt(s, candidates.Count());
			int temp         = candidates[s];
			candidates[s]    = candidates[pick];
			candidates[pick] = temp;

			if (s == 0)
				TrySpawnOnRoute(candidates[0], playerPos);
			else
				m_CallQueue.CallLater(TrySpawnDelayed, s * m_iSpawnStaggerMs, false, candidates[s]);
		}
	}

	protected void TrySpawnDelayed(int routeIndex)
	{
		if (!Replication.IsServer() || m_aActiveVehicles.Count() >= m_iMaxActiveVehicles)
			return;

		foreach (INS_AmbientVehicleAgent active : m_aActiveVehicles)
		{
			if (active.m_iRouteIndex == routeIndex)
				return;
		}

		array<vector> playerPos = {};
		array<int>    playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (controlled)
				playerPos.Insert(controlled.GetOrigin());
		}

		if (!playerPos.IsEmpty())
			TrySpawnOnRoute(routeIndex, playerPos);
	}

	protected void TrySpawnOnRoute(int routeIndex, array<vector> playerPos)
	{
		INS_AmbientVehicleRoute route = m_aRoutes[routeIndex];

		float distToStart = INS_ZoneHelpers.NearestDist(route.m_vRoadStart, playerPos, m_fDeactivationRadius + 1);
		float distToEnd   = INS_ZoneHelpers.NearestDist(route.m_vRoadEnd, playerPos, m_fDeactivationRadius + 1);

		bool startValid = distToStart >= m_fMinSpawnDistance && distToStart <= m_fActivationRadius;
		bool endValid   = distToEnd   >= m_fMinSpawnDistance && distToEnd   <= m_fActivationRadius;

		vector spawnPos, destPos;
		bool   forward, chosen = false;

		if (startValid && endValid)
		{
			// Spawn at whichever endpoint is furthest from the player
			if (distToStart >= distToEnd)
			{
				spawnPos = route.m_vRoadStart;
				destPos  = route.m_vRoadEnd;
				forward  = true;
			}
			else
			{
				spawnPos = route.m_vRoadEnd;
				destPos  = route.m_vRoadStart;
				forward  = false;
			}
			chosen = true;
		}
		else if (startValid)
		{
			spawnPos = route.m_vRoadStart;
			destPos  = route.m_vRoadEnd;
			forward  = true;
			chosen   = true;
		}
		else if (endValid)
		{
			spawnPos = route.m_vRoadEnd;
			destPos  = route.m_vRoadStart;
			forward  = false;
			chosen   = true;
		}

		if (!chosen)
			return;

		// Don't spawn on top of an existing vehicle
		foreach (INS_AmbientVehicleAgent active : m_aActiveVehicles)
		{
			if (!active.m_VehicleEntity)
				continue;

			vector diff = spawnPos - active.m_VehicleEntity.GetOrigin();
			if (diff.Length() < 50.0)
				return;
		}

		SpawnVehicle(spawnPos, destPos, routeIndex, forward);
	}

	// Vehicle spawn and despawn.
	protected void SpawnVehicle(vector spawnPos, vector destPos, int routeIndex, bool forward)
	{
		ResourceName prefab = m_aVehiclePrefabs[Math.RandomInt(0, m_aVehiclePrefabs.Count())];
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_AmbientVehicleManager] ERROR: Cannot load vehicle prefab: " + prefab);
			return;
		}

		// Orient the vehicle toward its destination
		vector dir = destPos - spawnPos;
		dir[1] = 0;
		dir.Normalize();

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode    = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]     = spawnPos;
		params.Transform[2]     = dir;

		vector up              = Vector(0, 1, 0);
		vector right           = up * dir;
		params.Transform[0]    = right;
		params.Transform[1]    = up;

		IEntity vehicleEnt = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!vehicleEnt)
		{
			Print("[INS_AmbientVehicleManager] ERROR: SpawnEntityPrefab failed.");
			return;
		}

		// Routes rebuild on start, so a persisted vehicle would stack on top of the one the tick spawns.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence)
			persistence.StopTracking(vehicleEnt, true);

		INS_AmbientVehicleAgent agent = new INS_AmbientVehicleAgent(vehicleEnt, destPos, routeIndex, forward);
		m_aActiveVehicles.Insert(agent);

		m_CallQueue.CallLater(SpawnCrew,        500,  false, agent);
		m_CallQueue.CallLater(AcquireCrewGroup, 1000, false, agent);
		m_CallQueue.CallLater(AssignDriveWaypoint, 1500, false, agent);

		string fromName, toName;
		if (forward)
		{
			fromName = m_aRoutes[routeIndex].m_sStartName;
			toName   = m_aRoutes[routeIndex].m_sEndName;
		}
		else
		{
			fromName = m_aRoutes[routeIndex].m_sEndName;
			toName   = m_aRoutes[routeIndex].m_sStartName;
		}
		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientVehicleManager] Vehicle spawned: '%1' -> '%2'", fromName, toName));
	}

	// Called 500ms after spawn fills crew compartments via the vehicle prefab defaults
	protected void SpawnCrew(INS_AmbientVehicleAgent agent)
	{
		if (!agent.m_bActive || !agent.m_VehicleEntity)
			return;

		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(
			agent.m_VehicleEntity.FindComponent(SCR_DamageManagerComponent));
		if (dmg && dmg.GetHealthScaled() <= 0)
		{
			if (m_bDebugLog)
				Print("[INS_AmbientVehicleManager] Vehicle was destroyed before crew could spawn. Skipping.");
			return;
		}

		vector mat[4];
		agent.m_VehicleEntity.GetTransform(mat);
		if (mat[1][1] < 0.7)
		{
			if (m_bDebugLog)
				Print("[INS_AmbientVehicleManager] Vehicle tipped before crew could spawn. Skipping.");
			return;
		}

		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
			agent.m_VehicleEntity.FindComponent(SCR_BaseCompartmentManagerComponent));

		if (!compMgr)
		{
			Print("[INS_AmbientVehicleManager] ERROR: No compartment manager on vehicle — despawning.");
			DespawnVehicle(agent);
			return;
		}

		bool ok = compMgr.SpawnDefaultOccupants(SCR_BaseCompartmentManagerComponent.CREW_COMPARTMENT_TYPES);
		if (!ok && m_bDebugLog)
			Print("[INS_AmbientVehicleManager] WARNING: SpawnDefaultOccupants returned false.");
	}

	// Called 1000ms after spawn  activates AI on each crew member and resolves their SCR_AIGroup
	protected void AcquireCrewGroup(INS_AmbientVehicleAgent agent)
	{
		if (!agent.m_bActive || !agent.m_VehicleEntity)
			return;

		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
			agent.m_VehicleEntity.FindComponent(SCR_BaseCompartmentManagerComponent));

		if (!compMgr)
			return;

		array<IEntity> occupants = {};
		compMgr.GetOccupants(occupants);

		if (occupants.IsEmpty())
		{
			if (m_bDebugLog)
				Print("[INS_AmbientVehicleManager] WARNING: No crew occupants found.");
			return;
		}

		PersistenceSystem persistence = PersistenceSystem.GetInstance();

		foreach (IEntity occ : occupants)
		{
			// Crew members need untracking for the same reason as the vehicle.
			if (persistence)
				persistence.StopTracking(occ, true);

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(occ);
			if (!character)
				continue;

			AIControlComponent ctrl = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
			if (!ctrl)
				continue;

			ctrl.ActivateAI();

			AIAgent aiAgent = ctrl.GetAIAgent();
			if (!aiAgent)
				continue;

			AIGroup grp = aiAgent.GetParentGroup();
			SCR_AIGroup scrGroup = SCR_AIGroup.Cast(grp);
			if (!scrGroup)
				continue;

			// Same goes for the crew group.
			if (persistence)
				persistence.StopTracking(scrGroup, true);

			agent.m_CrewGroup    = scrGroup;
			agent.m_bEverHadCrew = true;
			if (m_bDebugLog)
				Print("[INS_AmbientVehicleManager] Crew group acquired.");
			return;
		}

		if (m_bDebugLog)
			Print("[INS_AmbientVehicleManager] WARNING: Could not resolve crew SCR_AIGroup from any occupant.");
	}

	// Called 1500ms after spawn assigns the drive waypoint to the crew group
	protected void AssignDriveWaypoint(INS_AmbientVehicleAgent agent)
	{
		if (!agent.m_bActive)
			return;

		if (!agent.m_CrewGroup)
		{
			if (m_bDebugLog)
				Print("[INS_AmbientVehicleManager] WARNING: No crew group at waypoint assignment — vehicle may not drive.");
			return;
		}

		Resource wpRes = Resource.Load(INS_MOVE_WAYPOINT_PREFAB);
		if (!wpRes || !wpRes.IsValid())
		{
			Print("[INS_AmbientVehicleManager] ERROR: Cannot load move waypoint prefab.");
			DespawnVehicle(agent);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode    = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]     = agent.m_vDestination;

		IEntity wpEnt = GetGame().SpawnEntityPrefab(wpRes, GetGame().GetWorld(), params);
		if (!wpEnt)
		{
			Print("[INS_AmbientVehicleManager] ERROR: Failed to spawn drive waypoint.");
			DespawnVehicle(agent);
			return;
		}

		AIWaypoint wp = AIWaypoint.Cast(wpEnt);
		if (!wp)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(wpEnt);
			DespawnVehicle(agent);
			return;
		}

		agent.m_CrewGroup.AddWaypoint(wp);
		agent.m_CurrentWaypoint = wpEnt;

		if (m_bDebugLog)
			Print(string.Format("[INS_AmbientVehicleManager] Drive waypoint set at %1.", agent.m_vDestination.ToString()));
	}

	// Sends the vehicle back the other way along its route
	protected void RerouteVehicle(INS_AmbientVehicleAgent agent)
	{
		if (!agent.m_bActive || !agent.m_CrewGroup)
			return;

		if (agent.m_CurrentWaypoint)
		{
			AIWaypoint oldWp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
			if (oldWp)
				agent.m_CrewGroup.RemoveWaypoint(oldWp);

			SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
			agent.m_CurrentWaypoint = null;
		}

		// Flip direction
		if (agent.m_iRouteIndex >= 0 && agent.m_iRouteIndex < m_aRoutes.Count())
		{
			INS_AmbientVehicleRoute route = m_aRoutes[agent.m_iRouteIndex];
			if (agent.m_bForward)
			{
				agent.m_vDestination = route.m_vRoadStart;
				agent.m_bForward     = false;
			}
			else
			{
				agent.m_vDestination = route.m_vRoadEnd;
				agent.m_bForward     = true;
			}
		}

		agent.m_fStuckTime = 0;
		AssignDriveWaypoint(agent);

		if (m_bDebugLog)
			Print("[INS_AmbientVehicleManager] Vehicle rerouted.");
	}

	protected void DespawnVehicle(INS_AmbientVehicleAgent agent)
	{
		agent.m_bActive = false;

		if (agent.m_CurrentWaypoint)
		{
			if (agent.m_CrewGroup)
			{
				AIWaypoint wp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
				if (wp)
					agent.m_CrewGroup.RemoveWaypoint(wp);
			}

			SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
			agent.m_CurrentWaypoint = null;
		}

		if (agent.m_VehicleEntity)
		{
			if (HasPlayerOccupant(agent.m_VehicleEntity))
			{
				// Player has taken the vehicle stop tracking it but leave it intact
				m_aActiveVehicles.RemoveItem(agent);
				return;
			}

			SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
				agent.m_VehicleEntity.FindComponent(SCR_BaseCompartmentManagerComponent));

			if (compMgr)
			{
				array<IEntity> occupants = {};
				compMgr.GetOccupants(occupants);
				foreach (IEntity occ : occupants)
				{
					if (occ)
						SCR_EntityHelper.DeleteEntityAndChildren(occ);
				}
			}

			SCR_EntityHelper.DeleteEntityAndChildren(agent.m_VehicleEntity);
		}

		m_aActiveVehicles.RemoveItem(agent);
	}

	// Helpers.
	protected bool HasPlayerOccupant(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));

		if (!compMgr)
			return false;

		array<IEntity> occupants = {};
		compMgr.GetOccupants(occupants);

		foreach (IEntity occ : occupants)
		{
			if (!occ)
				continue;

			int pid = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(occ);
			if (pid > 0)
				return true;
		}

		return false;
	}

}
