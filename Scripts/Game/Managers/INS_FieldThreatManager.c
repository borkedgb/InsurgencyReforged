/**
 * INS_FieldThreatManager
 * by borked.gb
 *
 * Spawns stalkr patrols on player groups out in the open between settlements.
 * A patrol spawns at a random LOS-blocked position in a configurable ring around
 * the group and is given a Move waypoint at the group centroid. Waypoints refresh
 * on a random timer so the patrol tracks the group without stopping constantly.
 *
 * Two despawn triggers keep orphaned patrols from lingering:
 *   - Flee: group centroid shifts more than m_fFleeDistance between ticks (vehicle escape).
 *   - Abandon: patrol drifts further than m_fAbandonRange from the group centroid.
 *
 * After any despawn or kill a fresh cooldown starts, before the next patrol is queued.
 *
 * Exclusions (neither spawn nor track):
 *   - Players inside an INS_SafeZone (base or spawn area).
 *   - Players inside any active INS_AmbientPatrolManager zone (in a settlement).
 *
 * Setup:
 *   1. Attach to the GameMode entity.
 *   2. Set m_aGroupPrefabs to enemy infantry group prefabs.
 *   3. Place at least one INS_SafeZone at the spawn or on a generic entity.
 */

// Per-cluster threat state. One entry exists per active player cluster.
class INS_FieldThreatEntry
{
	ref array<int>      m_aPlayerIds;      // players currently in this cluster
	ref INS_PatrolAgent m_Patrol;          // null = no active patrol
	bool                m_bReadyToSpawn;   // true once the cooldown has elapsed
	bool                m_bEverHadLiving;  // guards against false death detection before spawn completes
	vector              m_vTargetPos;      // cluster centroid, updated every tick

	void INS_FieldThreatEntry(array<int> playerIds, vector centroid)
	{
		m_aPlayerIds     = new array<int>();
		foreach (int pid : playerIds)
			m_aPlayerIds.Insert(pid);
		m_Patrol          = null;
		m_bReadyToSpawn   = false;
		m_bEverHadLiving  = false;
		m_vTargetPos      = centroid;
	}
}

[ComponentEditorProps(category: "Insurgency", description: "Spawns stalker patrols on player groups out in the open. Pressure between objectives.")]
class INS_FieldThreatManagerClass : SCR_BaseGameModeComponentClass {}

class INS_FieldThreatManager : SCR_BaseGameModeComponent
{
	[Attribute()]
	protected ref array<ResourceName> m_aGroupPrefabs;

	[Attribute("1", UIWidgets.CheckBox, "Enable field threat patrols.")]
	protected bool m_bEnabled;

	[Attribute("200", UIWidgets.EditBox,
		"Players within this distance (m) of each other are treated as one group.")]
	protected float m_fGroupRadius;

	[Attribute("300", UIWidgets.EditBox,
		"Minimum spawn distance (m) from the target group centroid.")]
	protected float m_fMinSpawnDist;

	[Attribute("500", UIWidgets.EditBox,
		"Maximum spawn distance (m) from the target group centroid.")]
	protected float m_fMaxSpawnDist;

	[Attribute("30", UIWidgets.EditBox,
		"Minimum seconds between waypoint updates. Too short causes the AI to pause and recalculate constantly.")]
	protected float m_fMinWaypointRefresh;

	[Attribute("120", UIWidgets.EditBox,
		"Maximum seconds between waypoint updates.")]
	protected float m_fMaxWaypointRefresh;

	[Attribute("600", UIWidgets.EditBox,
		"Minimum seconds before a patrol spawns on a group (per group cooldown).")]
	protected float m_fMinCooldown;

	[Attribute("1200", UIWidgets.EditBox,
		"Maximum seconds before a patrol spawns on a group (per group cooldown).")]
	protected float m_fMaxCooldown;

	[Attribute("100", UIWidgets.EditBox,
		"If the group centroid moves this far (m) between ticks the patrol despawns. Catches vehicle escapes.")]
	protected float m_fFleeDistance;

	[Attribute("600", UIWidgets.EditBox,
		"If the patrol is further than this (m) from the group centroid it is abandoned and despawned.")]
	protected float m_fAbandonRange;

	[Attribute("3", UIWidgets.EditBox,
		"Maximum number of field threat patrols active at the same time across all groups.")]
	protected int m_iMaxConcurrentPatrols;

	[Attribute("10", UIWidgets.EditBox,
		"Seconds between group checks.")]
	protected float m_fCheckInterval;

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging.")]
	protected bool m_bDebugLog;

	protected ref array<ref INS_FieldThreatEntry> m_aEntries   = {};
	protected ref array<vector>                   m_aSafeZoneCenters = {};
	protected ref array<float>                    m_aSafeZoneRadii   = {};
	protected INS_AmbientPatrolManager            m_AmbientManager;
	protected ref ScriptCallQueue                 m_CallQueue = new ScriptCallQueue();

	// Public accessors
	bool  IsEnabled()                        { return m_bEnabled; }
	int   GetMaxConcurrentPatrols()          { return m_iMaxConcurrentPatrols; }
	void  SetMaxConcurrentPatrols(int count) { m_iMaxConcurrentPatrols = count; }
	float GetMinCooldown()                   { return m_fMinCooldown; }
	void  SetMinCooldown(float val)          { m_fMinCooldown = val; }
	float GetMaxCooldown()                   { return m_fMaxCooldown; }
	void  SetMaxCooldown(float val)          { m_fMaxCooldown = val; }

	void SetEnabled(bool bEnabled)
	{
		if (m_bEnabled == bEnabled)
			return;

		m_bEnabled = bEnabled;

		if (!Replication.IsServer())
			return;

		if (!bEnabled)
		{
			m_CallQueue.Remove(TickCheck);
			DespawnAll();
		}
		else
		{
			m_CallQueue.CallLater(TickCheck, (int)(m_fCheckInterval * 1000), true);
		}
	}

	// Lifecycle

	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();

		if (!m_bEnabled)
			return;

		if (!m_aGroupPrefabs || m_aGroupPrefabs.IsEmpty())
		{
			Print("[INS_FieldThreatManager] WARNING: No group prefabs set, disabled.");
			return;
		}

		// Cache safe zone data so we avoid component lookups every tick
		array<INS_SafeZone> safeZones = {};
		INS_SafeZone.CollectZones(safeZones);
		foreach (INS_SafeZone sz : safeZones)
		{
			IEntity owner = sz.GetOwner();
			if (!owner)
				continue;
			m_aSafeZoneCenters.Insert(owner.GetOrigin());
			m_aSafeZoneRadii.Insert(sz.GetRadius());
		}

		// Grab the ambient patrol manager for settlement exclusion checks
		BaseGameMode gm = GetGame().GetGameMode();
		if (gm)
			m_AmbientManager = INS_AmbientPatrolManager.Cast(gm.FindComponent(INS_AmbientPatrolManager));

		Print(string.Format("[INS_FieldThreatManager] Started. %1 safe zone(s).", m_aSafeZoneCenters.Count()));

		GetGame().GetCallqueue().CallLater(TickCallQueue, 100, true);
		m_CallQueue.CallLater(TickCheck, (int)(m_fCheckInterval * 1000), true);
	}

	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().Remove(TickCallQueue);
		DespawnAll();
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

		// Don't stop the tick here, it has to keep firing so threats come back when
		// someone reconnects. We just clear out the live patrols.
		DespawnAll();
	}

	protected void ApplyMissionHeader()
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return;

		INS_MissionParams params = header.m_Insurgency;
		if (!params)
			return;

		m_bEnabled               = params.m_bFieldThreatEnabled;
		m_iMaxConcurrentPatrols  = params.m_iMaxConcurrentFieldPatrols;
		m_fMinCooldown           = params.m_fThreatMinCooldown;
		m_fMaxCooldown           = params.m_fThreatMaxCooldown;
	}

	protected void TickCallQueue()
	{
		m_CallQueue.Tick(0.1);
	}

	// Main tick

	protected void TickCheck()
	{
		if (!Replication.IsServer())
			return;

		// Collect eligible players, skipping anyone in a safe zone or active settlement
		array<int>    eligibleIds       = {};
		array<vector> eligiblePositions = {};

		array<int> allIds = {};
		GetGame().GetPlayerManager().GetPlayers(allIds);

		foreach (int pid : allIds)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (!controlled)
				continue;

			vector pos = controlled.GetOrigin();

			if (IsInSafeZone(pos))
				continue;

			if (m_AmbientManager && m_AmbientManager.IsPositionInActiveZone(pos))
				continue;

			eligibleIds.Insert(pid);
			eligiblePositions.Insert(pos);
		}

		// Build clusters from eligible players
		array<ref array<int>> clusters  = {};
		array<vector>         centroids = {};
		BuildClusters(eligibleIds, eligiblePositions, clusters, centroids);

		// Track which clusters already have an entry so we don't double-up
		array<bool> clusterCovered = {};
		for (int i = 0; i < clusters.Count(); i++)
			clusterCovered.Insert(false);

		// Process existing entries
		array<ref INS_FieldThreatEntry> toRemove = {};

		foreach (INS_FieldThreatEntry entry : m_aEntries)
		{
			// Find the cluster with the most matching player IDs
			int bestIdx     = -1;
			int bestOverlap = 0;

			for (int ci = 0; ci < clusters.Count(); ci++)
			{
				int overlap = 0;
				foreach (int pid : clusters[ci])
				{
					if (entry.m_aPlayerIds.Find(pid) != -1)
						overlap++;
				}
				if (overlap > bestOverlap)
				{
					bestOverlap = overlap;
					bestIdx     = ci;
				}
			}

			if (bestIdx == -1)
			{
				// All target players have left, died or gone safe. Clean up.
				DespawnPatrol(entry);
				toRemove.Insert(entry);
				continue;
			}

			vector newCentroid = centroids[bestIdx];

			// Flee check: a big centroid shift between ticks usually means the group got in a vehicle
			if (entry.m_Patrol)
			{
				float moved = vector.Distance(entry.m_vTargetPos, newCentroid);
				if (moved > m_fFleeDistance)
				{
					if (m_bDebugLog)
						Print(string.Format("[INS_FieldThreatManager] Group fled (%.0fm). Patrol despawned.", moved));

					DespawnPatrol(entry);
					UpdateEntryCluster(entry, clusters[bestIdx], newCentroid);
					ScheduleCooldown(entry);
					clusterCovered[bestIdx] = true;
					continue;
				}
			}

			UpdateEntryCluster(entry, clusters[bestIdx], newCentroid);
			clusterCovered[bestIdx] = true;

			if (entry.m_Patrol)
			{
				// Guard against the entity being deleted externally
				if (!entry.m_Patrol.m_GroupEntity || !entry.m_Patrol.m_Group)
				{
					entry.m_Patrol = null;
					ScheduleCooldown(entry);
					continue;
				}

				// Abandon check: patrol has wandered too far from the group
				float patrolDist = vector.Distance(entry.m_Patrol.m_GroupEntity.GetOrigin(), entry.m_vTargetPos);
				if (patrolDist > m_fAbandonRange)
				{
					if (m_bDebugLog)
						Print("[INS_FieldThreatManager] Patrol abandoned. Despawning.");

					DespawnPatrol(entry);
					ScheduleCooldown(entry);
					continue;
				}

				// Death check: all members killed
				array<AIAgent> agents = {};
				entry.m_Patrol.m_Group.GetAgents(agents);

				int living = 0;
				foreach (AIAgent a : agents)
				{
					IEntity agentEnt = a.GetControlledEntity();
					if (!agentEnt)
						continue;
					DamageManagerComponent dmg = DamageManagerComponent.Cast(agentEnt.FindComponent(DamageManagerComponent));
					if (dmg && !dmg.IsDestroyed())
						living++;
				}

				if (living > 0)
				{
					entry.m_bEverHadLiving = true;
				}
				else if (entry.m_bEverHadLiving)
				{
					if (m_bDebugLog)
						Print("[INS_FieldThreatManager] Patrol wiped. Starting cooldown.");

					CleanupDeadPatrol(entry);
					ScheduleCooldown(entry);
					continue;
				}

			}
			else if (entry.m_bReadyToSpawn)
			{
				TrySpawnForEntry(entry);
			}
		}

		foreach (INS_FieldThreatEntry e : toRemove)
		{
			int idx = m_aEntries.Find(e);
			if (idx >= 0)
				m_aEntries.Remove(idx);
		}

		// Create entries for clusters that have no tracking yet
		for (int ci = 0; ci < clusters.Count(); ci++)
		{
			if (clusterCovered[ci])
				continue;

			INS_FieldThreatEntry entry = new INS_FieldThreatEntry(clusters[ci], centroids[ci]);
			m_aEntries.Insert(entry);
			ScheduleCooldown(entry);

			if (m_bDebugLog)
				Print(string.Format("[INS_FieldThreatManager] New cluster (%1 player(s)) tracked at %2.",
					clusters[ci].Count(), centroids[ci].ToString()));
		}
	}

	// Spawning

	protected void TrySpawnForEntry(INS_FieldThreatEntry entry)
	{
		// Respect the global concurrent cap
		int active = 0;
		foreach (INS_FieldThreatEntry e : m_aEntries)
		{
			if (e.m_Patrol)
				active++;
		}

		if (active >= m_iMaxConcurrentPatrols)
		{
			if (m_bDebugLog)
				Print("[INS_FieldThreatManager] At max concurrent patrols, deferring.");
			return; // stay ready, try next tick
		}

		vector spawnPos = FindSpawnPosition(entry.m_vTargetPos);
		if (spawnPos == vector.Zero)
		{
			if (m_bDebugLog)
				Print("[INS_FieldThreatManager] No valid spawn position found, deferring.");
			return;
		}

		ResourceName prefab = m_aGroupPrefabs[Math.RandomInt(0, m_aGroupPrefabs.Count())];

		if (m_bDebugLog)
			Print("[INS_FieldThreatManager] Selected prefab: " + prefab);

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_FieldThreatManager] ERROR: Cannot load prefab: " + prefab);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode    = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]     = spawnPos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			Print("[INS_FieldThreatManager] ERROR: SpawnEntityPrefab returned null.");
			return;
		}

		INS_PatrolAgent agent = new INS_PatrolAgent(ent, entry.m_vTargetPos, 0);
		entry.m_Patrol          = agent;
		entry.m_bReadyToSpawn   = false;
		entry.m_bEverHadLiving  = false;

		vector targetSnapshot = entry.m_vTargetPos;
		m_CallQueue.CallLater(InitThreatGroup, 500, false, agent, targetSnapshot);

		float firstRefresh = m_fMinWaypointRefresh + Math.RandomFloat01() * (m_fMaxWaypointRefresh - m_fMinWaypointRefresh);
		m_CallQueue.CallLater(RefreshWaypointTick, (int)(firstRefresh * 1000), false, entry);

		if (m_bDebugLog)
			Print(string.Format("[INS_FieldThreatManager] Patrol spawned at %1, targeting %2.",
				spawnPos.ToString(), entry.m_vTargetPos.ToString()));
	}

	protected void InitThreatGroup(INS_PatrolAgent agent, vector targetPos)
	{
		if (!agent.m_bActive || !agent.m_GroupEntity)
			return;

		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(agent.m_GroupEntity);
		if (aiGroup)
			aiGroup.SpawnAllImmediately();

		RefreshWaypoint(agent, targetPos);
	}

	protected void RefreshWaypointTick(INS_FieldThreatEntry entry)
	{
		if (m_aEntries.Find(entry) < 0)
			return;

		if (!entry.m_Patrol || !entry.m_Patrol.m_Group)
			return; // patrol is gone, chain stops. TrySpawnForEntry starts a fresh chain on next spawn.

		RefreshWaypoint(entry.m_Patrol, entry.m_vTargetPos);

		float delay = m_fMinWaypointRefresh + Math.RandomFloat01() * (m_fMaxWaypointRefresh - m_fMinWaypointRefresh);
		m_CallQueue.CallLater(RefreshWaypointTick, (int)(delay * 1000), false, entry);
	}

	// Waypoint management

	protected void RefreshWaypoint(INS_PatrolAgent agent, vector targetPos)
	{
		if (!agent || !agent.m_Group)
			return;

		if (agent.m_CurrentWaypoint)
		{
			AIWaypoint oldWp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
			if (oldWp)
				agent.RemoveWaypoint(oldWp);
			SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
			agent.m_CurrentWaypoint = null;
		}

		IEntity wpEnt = SpawnMoveWaypointAt(targetPos);
		if (!wpEnt)
			return;

		AIWaypoint wp = AIWaypoint.Cast(wpEnt);
		if (wp)
			agent.AddWaypoint(wp);

		agent.m_CurrentWaypoint = wpEnt;
	}

	protected IEntity SpawnMoveWaypointAt(vector pos)
	{
		Resource res = Resource.Load(INS_MOVE_WAYPOINT_PREFAB);
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode    = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3]     = pos;

		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	// Patrol lifecycle

	protected void DespawnPatrol(INS_FieldThreatEntry entry)
	{
		if (!entry.m_Patrol)
			return;

		INS_PatrolAgent agent = entry.m_Patrol;
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

		entry.m_Patrol = null;
	}

	// Patrol died in combat. Entity is already gone, just tidy up the waypoint ref.
	protected void CleanupDeadPatrol(INS_FieldThreatEntry entry)
	{
		if (!entry.m_Patrol)
			return;

		INS_PatrolAgent agent = entry.m_Patrol;
		agent.m_bActive = false;

		if (agent.m_CurrentWaypoint)
		{
			AIWaypoint wp = AIWaypoint.Cast(agent.m_CurrentWaypoint);
			if (wp)
				agent.RemoveWaypoint(wp);
			SCR_EntityHelper.DeleteEntityAndChildren(agent.m_CurrentWaypoint);
			agent.m_CurrentWaypoint = null;
		}

		entry.m_Patrol = null;
	}

	protected void DespawnAll()
	{
		foreach (INS_FieldThreatEntry entry : m_aEntries)
			DespawnPatrol(entry);
		m_aEntries.Clear();
	}

	// Cooldown

	protected void ScheduleCooldown(INS_FieldThreatEntry entry)
	{
		entry.m_bReadyToSpawn  = false;
		entry.m_bEverHadLiving = false;

		float safeMin = Math.Min(m_fMinCooldown, m_fMaxCooldown);
		float safeMax = Math.Max(m_fMinCooldown, m_fMaxCooldown);
		float delay   = safeMin + Math.RandomFloat01() * (safeMax - safeMin);
		m_CallQueue.CallLater(MarkEntryReady, (int)(delay * 1000), false, entry);

		if (m_bDebugLog)
			Print(string.Format("[INS_FieldThreatManager] Cooldown: %.0fs.", delay));
	}

	protected void MarkEntryReady(INS_FieldThreatEntry entry)
	{
		// The entry may have been removed while the cooldown was running
		if (m_aEntries.Find(entry) < 0)
			return;

		entry.m_bReadyToSpawn = true;

		if (m_bDebugLog)
			Print("[INS_FieldThreatManager] Entry ready to spawn.");
	}

	// Clustering

	protected void BuildClusters(array<int> playerIds, array<vector> positions,
		array<ref array<int>> outClusters, array<vector> outCentroids)
	{
		int count = playerIds.Count();
		float groupRadiusSq = m_fGroupRadius * m_fGroupRadius;

		array<bool> claimed = {};
		for (int i = 0; i < count; i++)
			claimed.Insert(false);

		for (int i = 0; i < count; i++)
		{
			if (claimed[i])
				continue;

			ref array<int> cluster        = new array<int>();
			ref array<int> clusterIndices = new array<int>();
			cluster.Insert(playerIds[i]);
			clusterIndices.Insert(i);
			claimed[i] = true;

			// Single-linkage: pull in anyone within groupRadius of any current cluster member
			bool expanded = true;
			while (expanded)
			{
				expanded = false;
				for (int j = 0; j < count; j++)
				{
					if (claimed[j])
						continue;

					foreach (int memberIdx : clusterIndices)
					{
						if (vector.DistanceSq(positions[memberIdx], positions[j]) <= groupRadiusSq)
						{
							cluster.Insert(playerIds[j]);
							clusterIndices.Insert(j);
							claimed[j] = true;
							expanded   = true;
							break;
						}
					}
				}
			}

			// Centroid
			vector centroid = vector.Zero;
			foreach (int idx : clusterIndices)
				centroid = centroid + positions[idx];
			centroid = centroid * (1.0 / cluster.Count());
			centroid[1] = GetGame().GetWorld().GetSurfaceY(centroid[0], centroid[2]);

			outClusters.Insert(cluster);
			outCentroids.Insert(centroid);
		}
	}

	// Spawn position selection

	// Picks a position in the spawn ring that has no clear LOS to any player.
	// Falls back to any land position in the ring if no blocked spot is found.
	protected vector FindSpawnPosition(vector targetPos)
	{
		BaseWorld world      = GetGame().GetWorld();
		float     oceanY     = world.GetOceanBaseHeight();
		float     rangeDiff  = m_fMaxSpawnDist - m_fMinSpawnDist;

		// Collect eye-height positions of all players for LOS testing
		array<vector> playerEyes = {};
		array<int> pids = {};
		GetGame().GetPlayerManager().GetPlayers(pids);
		foreach (int pid : pids)
		{
			IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (!controlled)
				continue;
			vector origin = controlled.GetOrigin();
			playerEyes.Insert(Vector(origin[0], origin[1] + 1.5, origin[2]));
		}

		// First pass: positions with no clear LOS to any player
		for (int i = 0; i < 20; i++)
		{
			float angle = Math.RandomFloat01() * Math.PI * 2.0;
			float dist  = m_fMinSpawnDist + Math.RandomFloat01() * rangeDiff;
			float x     = targetPos[0] + dist * Math.Cos(angle);
			float z     = targetPos[2] + dist * Math.Sin(angle);
			float y     = world.GetSurfaceY(x, z);

			if (y <= oceanY + 1.0)
				continue;

			vector eye = Vector(x, y + 1.5, z);
			bool allBlocked = true;

			foreach (vector playerEye : playerEyes)
			{
				TraceParam trace = new TraceParam();
				trace.Start = eye;
				trace.End   = playerEye;
				trace.Flags = TraceFlags.WORLD;

				float t = world.TraceMove(trace, null);
				if (t >= 1.0)
				{
					// Clear LOS to at least one player, reject this position
					allBlocked = false;
					break;
				}
			}

			if (allBlocked)
				return Vector(x, y, z);
		}

		// Fallback: any land position in the ring, no LOS guarantee
		if (m_bDebugLog)
			Print("[INS_FieldThreatManager] WARNING: No LOS-blocked position found. Using fallback.");

		for (int i = 0; i < 10; i++)
		{
			float angle = Math.RandomFloat01() * Math.PI * 2.0;
			float dist  = m_fMinSpawnDist + Math.RandomFloat01() * rangeDiff;
			float x     = targetPos[0] + dist * Math.Cos(angle);
			float z     = targetPos[2] + dist * Math.Sin(angle);
			float y     = world.GetSurfaceY(x, z);

			if (y > oceanY + 1.0)
				return Vector(x, y, z);
		}

		return vector.Zero;
	}

	// Helpers

	protected void UpdateEntryCluster(INS_FieldThreatEntry entry, array<int> playerIds, vector centroid)
	{
		entry.m_vTargetPos = centroid;
		entry.m_aPlayerIds.Clear();
		foreach (int pid : playerIds)
			entry.m_aPlayerIds.Insert(pid);
	}

	protected bool IsInSafeZone(vector pos)
	{
		for (int i = 0; i < m_aSafeZoneCenters.Count(); i++)
		{
			float rSq = m_aSafeZoneRadii[i] * m_aSafeZoneRadii[i];
			if (vector.DistanceSq(pos, m_aSafeZoneCenters[i]) <= rSq)
				return true;
		}
		return false;
	}
}
