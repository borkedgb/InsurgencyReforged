/**
 * INS_CacheSpawnManager
 * by borked.gb
 *
 * Game mode component that spawns weapon caches at random locations on game start.
 * Place INS_CacheSpawnPoint in generic entities in towns in the World Editor to define valid
 * spawn locations. This manager picks m_iCacheCount of them at random and spawns
 * the configured cache prefab at each position.
 *
 * Fresh session (no save): positions are randomised, both saves written.
 * Crash recovery (cache save exists): same positions reloaded, intel markers restored.
 * Clean end (OnGameModeEnd): both saves deleted so the next session randomises fresh.
 *
 * Attach to the GameMode entity alongside INS_IntelManager.
 */

const string INS_CACHE_SAVE_DIR  = "$profile:INS_Saves";
const string INS_CACHE_SAVE_FILE = "$profile:INS_Saves/cache_positions.dat";

[ComponentEditorProps(category: "Insurgency", description: "Spawns weapon caches at random INS_CacheSpawnPoint locations on game start.")]
class INS_CacheSpawnManagerClass : SCR_BaseGameModeComponentClass
{
}

class INS_CacheSpawnManager : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab to spawn as the weapons cache.", "et")]
	protected ResourceName m_sCachePrefab;

	[Attribute("3", UIWidgets.EditBox, "How many caches to spawn (chosen randomly from available spawn points).")]
	protected int m_iCacheCount;

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (cache spawns and registration).")]
	protected bool m_bDebugLog = false;

	// Static accessor
	static INS_CacheSpawnManager GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		return INS_CacheSpawnManager.Cast(gameMode.FindComponent(INS_CacheSpawnManager));
	}

	protected void ApplyMissionHeader()
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return;

		INS_MissionParams params = header.m_Insurgency;
		if (!params)
			return;

		m_iCacheCount = params.m_iCacheCount;
	}

	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		ApplyMissionHeader();
		GetGame().GetCallqueue().CallLater(SpawnCaches, 500, false);
	}

	// Clean end: delete both saves so the next session gets fresh random positions,
	// and wipe session storage so players don't respawn at their last positions.
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (!Replication.IsServer())
			return;

		if (FileIO.FileExists(INS_CACHE_SAVE_FILE))
			FileIO.DeleteFile(INS_CACHE_SAVE_FILE);

		if (FileIO.FileExists(INS_INTEL_SAVE_FILE))
			FileIO.DeleteFile(INS_INTEL_SAVE_FILE);

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByCurrentWorld();
		if (persistence)
			persistence.ClearStorage(PersistenceSessionStorage);
	}

	protected void SpawnCaches()
	{
		if (m_sCachePrefab.IsEmpty())
		{
			Print("[INS_CacheSpawnManager] No cache prefab assigned! Set m_sCachePrefab in the editor.", LogLevel.WARNING);
			return;
		}

		array<vector> chosen = {};

		if (LoadCachePositions(chosen))
		{
			// Crash recovery: reuse the same positions so intel markers remain valid
			Print(string.Format("[INS_CacheSpawnManager] Crash recovery: reusing %1 saved cache position(s).", chosen.Count()));
		}
		else
		{
			// Fresh session: randomise and save
			array<vector> all = {};
			int count = INS_CacheSpawnPoint.CollectPositions(all);

			if (count == 0)
			{
				Print("[INS_CacheSpawnManager] No INS_CacheSpawnPoint entities found in the world!", LogLevel.WARNING);
				return;
			}

			// Fisher-Yates shuffle
			for (int i = all.Count() - 1; i > 0; i--)
			{
				int j = Math.RandomInt(0, i + 1);
				vector tmp = all[i];
				all[i] = all[j];
				all[j] = tmp;
			}

			int toSpawn = Math.Min(m_iCacheCount, all.Count());
			for (int i = 0; i < toSpawn; i++)
				chosen.Insert(all[i]);

			SaveCachePositions(chosen);
		}

		Print(string.Format("[INS_CacheSpawnManager] Spawning %1 cache(s).", chosen.Count()));

		foreach (vector pos : chosen)
			SpawnCacheAt(pos);
	}

	// Temp storage for the cache search callback, cleared after use
	protected IEntity m_FoundCache = null;

	protected void SpawnCacheAt(vector pos)
	{
		IEntity cache = FindExistingCacheNearPoint(pos, 2.0);

		if (cache)
		{
			if (m_bDebugLog)
				Print("[INS_CacheSpawnManager] Adopted persisted cache at: " + pos.ToString());
		}
		else
		{
			Resource res = Resource.Load(m_sCachePrefab);
			if (!res || !res.IsValid())
			{
				Print("[INS_CacheSpawnManager] Failed to load cache prefab: " + m_sCachePrefab, LogLevel.ERROR);
				return;
			}

			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;

			Math3D.MatrixIdentity4(params.Transform);
			params.Transform[3] = pos;

			cache = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			if (!cache)
			{
				Print("[INS_CacheSpawnManager] SpawnEntityPrefab returned null at: " + pos.ToString(), LogLevel.ERROR);
				return;
			}

			if (m_bDebugLog)
				Print("[INS_CacheSpawnManager] Cache spawned at: " + pos.ToString());
		}

		INS_IntelManager mgr = INS_IntelManager.GetInstance();
		if (mgr)
		{
			mgr.RegisterCache(cache, pos);
			if (m_bDebugLog)
				Print("[INS_CacheSpawnManager] Cache registered OK.");
		}
		else
		{
			Print("[INS_CacheSpawnManager] INS_IntelManager not found - cache not registered!");
		}
	}

	protected IEntity FindExistingCacheNearPoint(vector pos, float radius)
	{
		m_FoundCache = null;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, radius, CacheSearchCallback);
		IEntity result = m_FoundCache;
		m_FoundCache = null;
		return result;
	}

	protected bool CacheSearchCallback(IEntity ent)
	{
		if (!ent || m_FoundCache)
			return true;

		if (ent.FindComponent(INS_CacheComponent))
			m_FoundCache = ent;

		return true;
	}

	protected void SaveCachePositions(array<vector> positions)
	{
		FileIO.MakeDirectory(INS_CACHE_SAVE_DIR);

		FileSerializer file = new FileSerializer();
		if (!file.Open(INS_CACHE_SAVE_FILE, FileMode.WRITE))
		{
			Print("[INS_CacheSpawnManager] WARNING: Could not open cache position save file for writing.");
			return;
		}

		file.Write(positions.Count());
		foreach (vector pos : positions)
		{
			float x = pos[0];
			float y = pos[1];
			float z = pos[2];
			file.Write(x);
			file.Write(y);
			file.Write(z);
		}

		file.Close();
		Print(string.Format("[INS_CacheSpawnManager] Saved %1 cache position(s).", positions.Count()));
	}

	protected bool LoadCachePositions(out array<vector> outPos)
	{
		if (!FileIO.FileExists(INS_CACHE_SAVE_FILE))
			return false;

		FileSerializer file = new FileSerializer();
		if (!file.Open(INS_CACHE_SAVE_FILE, FileMode.READ))
			return false;

		int count;
		file.Read(count);

		for (int i = 0; i < count; i++)
		{
			float x, y, z;
			file.Read(x);
			file.Read(y);
			file.Read(z);
			outPos.Insert(Vector(x, y, z));
		}

		file.Close();

		if (outPos.IsEmpty())
			return false;

		return true;
	}
}
