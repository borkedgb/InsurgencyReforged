/**
 * INS_IntelManager
 * by borked.gb
 *
 * Game mode component that tracks intel collected by players.
 *
 * Mission flow:
 *   1. Players gather intel from enemy bodies.
 *   2. Each intel find targets a random undestroyed cache and drops a map marker
 *      progressively closer to it. All caches are in play simultaneously so players
 *      can't tell which cache a given marker belongs to; they have to triangulate.
 *   3. As intel accumulates for a cache, markers close in. Once the threshold is hit
 *      a broadcast hint fires ("a cache location has been narrowed down") without
 *      revealing which one.
 *   4. Players investigate the map, identify a cluster, destroy that cache.
 *      All markers for it are cleared.
 *   5. Repeat until all caches are gone, then mission complete.
 *
 * Intel count, per cache destroyed state, and all placed markers are saved to disk
 * after every change. On a server restart the state is restored once caches
 * have re-registered. Save is deleted on clean mission end (OnGameModeEnd).
 *
 * Attach to the GameMode entity alongside SCR_HintManagerComponent.
 * SCR_MapMarkerManagerComponent is already in GameMode_Base.et, no extra setup needed.
 */

const string INS_INTEL_SAVE_DIR  = "$profile:INS_Saves";
const string INS_INTEL_SAVE_FILE = "$profile:INS_Saves/intel_state.dat";

[ComponentEditorProps(category: "Insurgency", description: "Manages intel collection and cache revelation.")]
class INS_IntelManagerClass : SCR_BaseGameModeComponentClass
{
}

class INS_IntelManager : SCR_BaseGameModeComponent
{
	[Attribute("10", UIWidgets.EditBox, "Intel items required to fully reveal each cache location.")]
	protected int m_iIntelThreshold;

	[Attribute("2500", UIWidgets.EditBox, "Distance (m) of the first marker from a cache.")]
	protected int m_iMarkerDistMax;

	[Attribute("50", UIWidgets.EditBox, "Distance (m) of the final marker from a cache.")]
	protected int m_iMarkerDistMin;

	[Attribute("0", UIWidgets.CheckBox, "DEBUG: Place a marker at each cache's exact position. Only visible to logged-in admins.")]
	protected bool m_bDebugCacheMarkers;

	[Attribute("0.05", UIWidgets.EditBox, "Probability (0.0 - 1.0) that a body yields intel when searched.")]
	protected float m_fIntelDropChance;

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (cache registration, marker placement, intel counts).")]
	protected bool m_bDebugLog = false;

	// Parallel arrays, one entry per registered cache
	protected ref array<IEntity> m_aCacheEntities    = {};
	protected ref array<bool>    m_aCacheDestroyed   = {};
	protected ref array<bool>    m_aCacheRevealed    = {};
	protected ref array<int>     m_aCacheIntelCounts = {};

	// Marker data, parallel to m_aCacheEntities
	protected ref array<ref array<SCR_MapMarkerBase>> m_aCacheMarkers     = {};
	protected ref array<ref array<int>>               m_aCacheMarkerDists = {};
	protected ref array<ref array<int>>               m_aCacheMarkerWorldX = {};
	protected ref array<ref array<int>>               m_aCacheMarkerWorldZ = {};

	// Local only debug markers, tracked so they can be removed when the toggle is turned off
	protected ref array<SCR_MapMarkerBase> m_aDebugMarkers = {};

	// Cached marker type resolved from config on first use, config never changes at runtime
	protected SCR_EMapMarkerType m_ePlacedMarkerType;
	protected bool               m_bMarkerTypeCached = false;

	protected int m_iCachesDestroyed = 0;

	// Public accessors
	int   GetIntelThreshold()          { return m_iIntelThreshold; }
	void  SetIntelThreshold(int count) { m_iIntelThreshold = count; }

	float GetIntelDropChance()         { return m_fIntelDropChance; }
	void  SetIntelDropChance(float f)  { m_fIntelDropChance = f; }

	bool GetDebugCacheMarkers() { return m_bDebugCacheMarkers; }

	void SetDebugCacheMarkers(bool value)
	{
		m_bDebugCacheMarkers = value;
		if (!Replication.IsServer())
			return;

		if (value)
			BroadcastDebugMarkers();
		else
		{
			ClearDebugMarkers();
			Rpc(RpcClearDebugMarkers);
		}
	}

	// Static accessor
	static INS_IntelManager GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		return INS_IntelManager.Cast(gameMode.FindComponent(INS_IntelManager));
	}

	protected void ApplyMissionHeader()
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return;

		INS_MissionParams params = header.m_Insurgency;
		if (!params)
			return;

		m_iIntelThreshold  = params.m_iIntelThreshold;
		m_fIntelDropChance = params.m_fIntelDropChance;
	}

	// Defer load until after INS_CacheSpawnManager has finished spawning and registering caches
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();

		// Cache spawner uses a 500ms delay; 700ms gives it room to finish
		GetGame().GetCallqueue().CallLater(LoadIntelState, 700, false);

		// 800ms gives both SpawnCaches and LoadIntelState time to complete
		GetGame().GetCallqueue().CallLater(BroadcastDebugMarkers, 800, false);
	}

	// Called by INS_CacheSpawnManager to register a cache entity
	void RegisterCache(IEntity cacheEntity, vector pos)
	{
		if (!cacheEntity)
			return;

		if (m_aCacheEntities.Contains(cacheEntity))
			return;

		m_aCacheEntities.Insert(cacheEntity);
		m_aCacheDestroyed.Insert(false);
		m_aCacheRevealed.Insert(false);
		m_aCacheIntelCounts.Insert(0);
		m_aCacheMarkers.Insert(new array<SCR_MapMarkerBase>());
		m_aCacheMarkerDists.Insert(new array<int>());
		m_aCacheMarkerWorldX.Insert(new array<int>());
		m_aCacheMarkerWorldZ.Insert(new array<int>());

		if (m_bDebugLog)
			Print(string.Format("[INS_IntelManager] Cache registered. Total: %1", m_aCacheEntities.Count()));
	}

	// Legacy alias kept so INS_CacheComponent still compiles without changes
	void RegisterCachePosition(vector pos) {}

	bool IsRegisteredCache(IEntity entity)
	{
		return m_aCacheEntities.Contains(entity);
	}

	// Called by INS_BodySearchAction when intel is found (server-side).
	// Picks a random undestroyed cache and drops a marker for it.
	void AddIntelAndBroadcast(int amount, int searcherID = -1)
	{
		if (!Replication.IsServer())
			return;

		// Routing intel to a revealed cache would just pile markers at minimum distance, so we stop here.
		array<int> active = {};
		for (int i = 0; i < m_aCacheEntities.Count(); i++)
		{
			if (!m_aCacheDestroyed[i] && !m_aCacheRevealed[i])
				active.Insert(i);
		}

		if (active.IsEmpty())
			return;

		// Random cache gets this intel, players won't know which
		int pick = active[Math.RandomInt(0, active.Count())];
		m_aCacheIntelCounts.Set(pick, m_aCacheIntelCounts[pick] + amount);

		AddProgressMarker(pick);

		ShowFoundHint();
		Rpc(RpcBroadcastFoundHint);

		// First time this cache crosses the threshold, mark it and broadcast a reveal hint.
		if (m_aCacheIntelCounts[pick] >= m_iIntelThreshold)
		{
			m_aCacheRevealed.Set(pick, true);
			int remaining = m_aCacheEntities.Count() - m_iCachesDestroyed;
			ShowRevealHint(remaining);
			Rpc(RpcAnnounceReveal, remaining);
		}

		SaveIntelState();
	}

	// Legacy alias
	void AddIntel(int amount) { AddIntelAndBroadcast(amount); }

	// Called by INS_BodySearchAction when no intel was found (server-side)
	void BroadcastSearchFailed(int searcherID = -1)
	{
		if (!Replication.IsServer())
			return;

		// Show locally if the host player is the one who searched
		PlayerController localPc = GetGame().GetPlayerController();
		if (localPc && localPc.GetPlayerId() == searcherID)
			ShowSearchFailedHint();

		Rpc(RpcBroadcastSearchFailed, searcherID);
	}

	// Called by INS_CacheDestructionHook when a cache is destroyed (server-side)
	void OnCacheDestroyed(IEntity cacheEntity)
	{
		if (!Replication.IsServer())
			return;

		int idx = -1;
		for (int i = 0; i < m_aCacheEntities.Count(); i++)
		{
			if (m_aCacheEntities[i] == cacheEntity)
			{
				idx = i;
				break;
			}
		}

		if (idx < 0)
		{
			Print("[INS_IntelManager] OnCacheDestroyed: entity not in registry!");
			return;
		}

		RemoveAllCacheMarkers(idx);

		m_aCacheDestroyed[idx] = true;
		m_iCachesDestroyed++;

		if (m_bDebugLog)
			Print(string.Format("[INS_IntelManager] Caches destroyed: %1 / %2", m_iCachesDestroyed, m_aCacheEntities.Count()));

		ShowCacheDestroyedHint(m_iCachesDestroyed, m_aCacheEntities.Count());
		Rpc(RpcBroadcastCacheDestroyed, m_iCachesDestroyed, m_aCacheEntities.Count());

		SaveIntelState();

		if (m_iCachesDestroyed >= m_aCacheEntities.Count())
			GetGame().GetCallqueue().CallLater(TriggerMissionComplete, 5000, false);
	}

	// Places one marker for the given cache. Distance interpolates from max (first intel) to
	// min (threshold intel), closing in gradually. Retries random angles to avoid placing at sea.
	protected void AddProgressMarker(int cacheIndex)
	{
		IEntity cacheEnt = m_aCacheEntities[cacheIndex];
		if (!cacheEnt)
			return;

		vector cachePos = cacheEnt.GetOrigin();

		int cacheIntel = m_aCacheIntelCounts[cacheIndex];
		int dist;
		if (m_iIntelThreshold <= 1)
			dist = m_iMarkerDistMin;
		else
			dist = m_iMarkerDistMax - (m_iMarkerDistMax - m_iMarkerDistMin) * (cacheIntel - 1) / (m_iIntelThreshold - 1);

		dist = Math.Clamp(dist, m_iMarkerDistMin, m_iMarkerDistMax);

		BaseWorld world = GetGame().GetWorld();

		const int MAX_ATTEMPTS = 20;
		int markerX, markerZ;
		bool foundLand = false;

		for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
		{
			float angle = Math.RandomFloat01() * Math.PI * 2.0;
			markerX = (int)cachePos[0] + (int)(dist * Math.Cos(angle));
			markerZ = (int)cachePos[2] + (int)(dist * Math.Sin(angle));

			if (world.GetSurfaceY(markerX, markerZ) > 1.0)
			{
				foundLand = true;
				break;
			}
		}

		if (!foundLand)
		{
			float fallbackAngle = Math.RandomFloat01() * Math.PI * 2.0;
			int stepDist = dist;
			while (stepDist > 0)
			{
				stepDist -= 50;
				markerX = (int)cachePos[0] + (int)(stepDist * Math.Cos(fallbackAngle));
				markerZ = (int)cachePos[2] + (int)(stepDist * Math.Sin(fallbackAngle));

				if (world.GetSurfaceY(markerX, markerZ) > 1.0)
				{
					foundLand = true;
					break;
				}
			}
		}

		if (!foundLand)
		{
			markerX = (int)cachePos[0];
			markerZ = (int)cachePos[2];
			if (m_bDebugLog)
				Print(string.Format("[INS_IntelManager] Cache %1: all fallback positions at sea, using cache origin", cacheIndex));
		}

		CreateCacheMarker(markerX, markerZ, dist, cacheIndex);

		if (m_bDebugLog)
			Print(string.Format("[INS_IntelManager] Cache %1: intel %2/%3, marker ~%4m (land: %5)",
				cacheIndex, cacheIntel, m_iIntelThreshold, dist, foundLand));
	}

	// Broadcasts exact cache positions to all clients, should be to admins only but I havent tested it properly (i'll be back)
	protected void BroadcastDebugMarkers()
	{
		if (!m_bDebugCacheMarkers)
			return;

		for (int i = 0; i < m_aCacheEntities.Count(); i++)
		{
			IEntity cache = m_aCacheEntities[i];
			if (!cache)
				continue;

			vector pos = cache.GetOrigin();
			PlaceDebugMarker((int)pos[0], (int)pos[2], i);
			Rpc(RpcPlaceDebugMarker, (int)pos[0], (int)pos[2], i);
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcPlaceDebugMarker(int worldX, int worldZ, int cacheIndex)
	{
		PlaceDebugMarker(worldX, worldZ, cacheIndex);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcClearDebugMarkers()
	{
		ClearDebugMarkers();
	}

	protected void ClearDebugMarkers()
	{
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerMgr)
		{
			foreach (SCR_MapMarkerBase marker : m_aDebugMarkers)
			{
				if (marker)
					markerMgr.RemoveStaticMarker(marker);
			}
		}

		m_aDebugMarkers.Clear();
	}

	protected SCR_EMapMarkerType ResolveMarkerType()
	{
		if (m_bMarkerTypeCached)
			return m_ePlacedMarkerType;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
			return m_ePlacedMarkerType;

		SCR_MapMarkerConfig markerConfig = markerMgr.GetMarkerConfig();
		if (!markerConfig)
			return m_ePlacedMarkerType;

		array<ref SCR_MapMarkerEntryConfig> entries = markerConfig.GetMarkerEntryConfigs();
		if (entries)
		{
			foreach (SCR_MapMarkerEntryConfig entry : entries)
			{
				if (SCR_MapMarkerEntryPlaced.Cast(entry))
				{
					m_ePlacedMarkerType  = entry.GetMarkerType();
					m_bMarkerTypeCached  = true;
					break;
				}
			}
		}

		return m_ePlacedMarkerType;
	}

	protected void PlaceDebugMarker(int worldX, int worldZ, int cacheIndex)
	{
		int localId = SCR_PlayerController.GetLocalPlayerId();
		EPlayerRole roles = GetGame().GetPlayerManager().GetPlayerRoles(localId);
		if (!SCR_Global.IsAdminRole(roles))
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
			return;

		SCR_EMapMarkerType placedType = ResolveMarkerType();

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(placedType);
		marker.SetIconEntry(18);
		marker.SetWorldPos(worldX, worldZ);
		marker.SetCustomText(string.Format("[DBG CACHE %1]", cacheIndex + 1));
		marker.SetMarkerOwnerID(-1);
		marker.SetCanBeRemovedByOwner(false);
		markerMgr.InsertStaticMarker(marker, true, false);
		m_aDebugMarkers.Insert(marker);
	}

	protected void CreateCacheMarker(int worldX, int worldZ, int distMetres, int cacheIndex)
	{
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
		{
			Print("[INS_IntelManager] CreateCacheMarker: SCR_MapMarkerManagerComponent not found!");
			return;
		}

		SCR_EMapMarkerType placedType = ResolveMarkerType();
		if (!m_bMarkerTypeCached)
		{
			Print("[INS_IntelManager] CreateCacheMarker: SCR_MapMarkerEntryPlaced not found in config!");
			return;
		}

		int labelDist = distMetres;
		if (cacheIndex >= 0 && cacheIndex < m_aCacheEntities.Count())
		{
			IEntity cacheEnt = m_aCacheEntities[cacheIndex];
			if (cacheEnt)
				labelDist = (int)vector.Distance(cacheEnt.GetOrigin(), Vector(worldX, 0, worldZ));
		}

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(placedType);
		marker.SetIconEntry(18);
		marker.SetWorldPos(worldX, worldZ);
		marker.SetCustomText(string.Format("~%1m", labelDist));
		marker.SetMarkerOwnerID(-1);
		marker.SetCanBeRemovedByOwner(false);

		markerMgr.InsertStaticMarker(marker, false, true);

		if (cacheIndex >= 0 && cacheIndex < m_aCacheMarkers.Count())
		{
			m_aCacheMarkers[cacheIndex].Insert(marker);
			m_aCacheMarkerDists[cacheIndex].Insert(distMetres);
			m_aCacheMarkerWorldX[cacheIndex].Insert(worldX);
			m_aCacheMarkerWorldZ[cacheIndex].Insert(worldZ);
		}

		if (m_bDebugLog)
			Print(string.Format("[INS_IntelManager] Marker added for cache %1 (~%2m, total: %3)",
				cacheIndex, distMetres, m_aCacheMarkers[cacheIndex].Count()));
	}

	protected void RemoveAllCacheMarkers(int cacheIndex)
	{
		if (cacheIndex < 0 || cacheIndex >= m_aCacheMarkers.Count())
			return;

		array<SCR_MapMarkerBase> markers = m_aCacheMarkers[cacheIndex];
		if (!markers || markers.IsEmpty())
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerMgr)
		{
			foreach (SCR_MapMarkerBase marker : markers)
			{
				if (marker)
					markerMgr.RemoveStaticMarker(marker);
			}
		}

		markers.Clear();
		m_aCacheMarkerDists[cacheIndex].Clear();
		m_aCacheMarkerWorldX[cacheIndex].Clear();
		m_aCacheMarkerWorldZ[cacheIndex].Clear();
	}

	protected void TriggerMissionComplete()
	{
		Print("[INS_IntelManager] All caches destroyed - triggering mission complete.");

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("[INS_IntelManager] ERROR: Could not get SCR_BaseGameMode.");
			return;
		}

		gameMode.EndGameMode(SCR_GameModeEndData.CreateSimple(EGameOverTypes.FACTION_VICTORY_TIME));
	}

	// Display helpers
	protected void ShowFoundHint()
	{
		SCR_HintManagerComponent.ShowCustomHint(
			"Intel secured. A new marker has appeared on your map.",
			"Intel Found",
			5.0,
			false
		);
	}

	protected void ShowSearchFailedHint()
	{
		SCR_HintManagerComponent.ShowCustomHint(
			"Nothing useful found on this body.",
			"Search Result",
			4.0,
			false
		);
	}

	protected void ShowRevealHint(int remaining)
	{
		SCR_HintManagerComponent.ShowCustomHint(
			string.Format("A cache location has been narrowed down. Check your map. (%1 cache(s) remaining)", remaining),
			"Cache Location Revealed!",
			8.0,
			false
		);
	}

	protected void ShowCacheDestroyedHint(int destroyed, int total)
	{
		string msg;
		if (destroyed >= total)
			msg = "All weapons caches have been destroyed! Mission complete!";
		else
			msg = string.Format("Weapons cache destroyed! (%1/%2 down). Keep gathering intel.", destroyed, total);

		SCR_HintManagerComponent.ShowCustomHint(
			msg,
			"Cache Destroyed!",
			8.0,
			false
		);
	}

	// RPCs
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcBroadcastFoundHint()
	{
		ShowFoundHint();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcBroadcastSearchFailed(int searcherID)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc || pc.GetPlayerId() != searcherID)
			return;

		ShowSearchFailedHint();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcAnnounceReveal(int remaining)
	{
		ShowRevealHint(remaining);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcBroadcastCacheDestroyed(int destroyed, int total)
	{
		ShowCacheDestroyedHint(destroyed, total);
	}

	// Persistence
	protected void SaveIntelState()
	{
		FileIO.MakeDirectory(INS_INTEL_SAVE_DIR);

		FileSerializer file = new FileSerializer();
		if (!file.Open(INS_INTEL_SAVE_FILE, FileMode.WRITE))
		{
			Print("[INS_IntelManager] WARNING: Could not open intel save file for writing.");
			return;
		}

		file.Write(m_iCachesDestroyed);

		int numCaches = m_aCacheEntities.Count();
		file.Write(numCaches);

		for (int ci = 0; ci < numCaches; ci++)
		{
			int destroyedInt = 0;
			if (m_aCacheDestroyed[ci])
				destroyedInt = 1;
			file.Write(destroyedInt);
			file.Write(m_aCacheIntelCounts[ci]);

			array<int> wxArr  = m_aCacheMarkerWorldX[ci];
			array<int> wzArr  = m_aCacheMarkerWorldZ[ci];
			array<int> dists  = m_aCacheMarkerDists[ci];
			int numMarkers = dists.Count();
			file.Write(numMarkers);

			for (int mi = 0; mi < numMarkers; mi++)
			{
				file.Write(wxArr[mi]);
				file.Write(wzArr[mi]);
				file.Write(dists[mi]);
			}
		}

		file.Close();
		Print(string.Format("[INS_IntelManager] State saved. %1/%2 caches destroyed.",
			m_iCachesDestroyed, m_aCacheEntities.Count()));
	}

	protected void LoadIntelState()
	{
		if (!FileIO.FileExists(INS_INTEL_SAVE_FILE))
			return;

		FileSerializer file = new FileSerializer();
		if (!file.Open(INS_INTEL_SAVE_FILE, FileMode.READ))
			return;

		file.Read(m_iCachesDestroyed);

		int numCaches;
		file.Read(numCaches);

		for (int ci = 0; ci < numCaches; ci++)
		{
			int destroyedInt;
			file.Read(destroyedInt);
			if (ci < m_aCacheDestroyed.Count())
				m_aCacheDestroyed[ci] = (destroyedInt != 0);

			int intelCount;
			file.Read(intelCount);
			if (ci < m_aCacheIntelCounts.Count())
				m_aCacheIntelCounts[ci] = intelCount;

			// Reveal state isn't saved separately, derive it from the loaded count.
			if (ci < m_aCacheRevealed.Count() && intelCount >= m_iIntelThreshold && !m_aCacheDestroyed[ci])
				m_aCacheRevealed.Set(ci, true);

			int numMarkers;
			file.Read(numMarkers);

			for (int mi = 0; mi < numMarkers; mi++)
			{
				int wx, wz, dist;
				file.Read(wx);
				file.Read(wz);
				file.Read(dist);

				if (ci < m_aCacheMarkers.Count())
					CreateCacheMarker(wx, wz, dist, ci);
			}
		}

		file.Close();

		Print(string.Format("[INS_IntelManager] State restored. %1/%2 caches destroyed.",
			m_iCachesDestroyed, m_aCacheEntities.Count()));
	}
}
