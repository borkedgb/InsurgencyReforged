/**
 * INS_VehicleRespawner
 * by borked.gb
 *
 * Attach to a GenericEntity at the vehicle's spawn point, not the vehicle itself.
 * The component spawns the first vehicle on game start and respawns it whenever it is
 * destroyed or garbage collected. Because the spawn point entity is never GC'd, the
 * chain runs indefinitely regardless of where the vehicle ends up.
 *
 * Setup:
 *   1. Place a GenericEntity at the spawn location in the World Editor.
 *   2. Add this component to that GenericEntity (not to a vehicle).
 *   3. Set m_sVehiclePrefab to the vehicle to spawn.
 *   4. Match this to the prefab so the component picks the right delay from the mission header.
 *   5. Tune m_iMaxRespawns and m_fPollInterval as needed.
 *   6. Delays come from the mission header unless you set m_fRespawnDelay, which overrides for this instance only.
 *   7. No vehicle needs to be pre-placed - the component spawns the first one on game start.
 */

[ComponentEditorProps(category: "Insurgency", description: "Attach to a spawn point GenericEntity. Spawns and respawns a vehicle at this location when destroyed or GC'd.")]
class INS_VehicleRespawnerClass : ScriptComponentClass {}

enum EINS_VehicleType
{
	Wheeled,
	Armour,
	Aircraft
}

class INS_VehicleRespawner : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker,
		"Vehicle prefab to spawn and respawn.", "et")]
	protected ResourceName m_sVehiclePrefab;

	[Attribute("0", UIWidgets.ComboBox,
		"Vehicle type, used to look up the respawn delay in the mission header.",
		"", ParamEnumArray.FromEnum(EINS_VehicleType))]
	protected EINS_VehicleType m_eVehicleType;

	[Attribute("0", UIWidgets.EditBox,
		"How long to wait after destruction before the next vehicle spawns. 0 uses the mission header value for this type.")]
	protected float m_fRespawnDelay;

	[Attribute("-1", UIWidgets.EditBox,
		"How many times the vehicle can be replaced. -1 means unlimited.")]
	protected int m_iMaxRespawns;

	[Attribute("5", UIWidgets.EditBox,
		"How often (seconds) to check if the vehicle has been destroyed.")]
	protected float m_fPollInterval;

	[Attribute("3", UIWidgets.EditBox,
		"Radius (metres) around the spawn point to sweep for vehicle wrecks before spawning.")]
	protected float m_fWreckCleanupRadius;

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging (spawns, destruction, wrecks).")]
	protected bool m_bDebugLog = false;

	protected ref ScriptCallQueue m_CallQueue = new ScriptCallQueue();

	// Spawn point transform cached from the owner entity on init
	protected vector m_vSpawnTransform[4];
	protected int    m_iRespawnCount = 0;
	protected bool   m_bPendingSpawn = false;

	// Weak reference to whichever vehicle is currently live. Goes null when GC'd or deleted.
	protected IEntity m_CurrentVehicle = null;

	// Temp list used during the wreck sweep callback, cleared immediately after
	protected ref array<IEntity> m_aWrecksToDelete = null;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		// Cache the spawn point transform, this is where every vehicle will appear
		owner.GetTransform(m_vSpawnTransform);

		// Start polling before the first spawn so we don't miss a window
		GetGame().GetCallqueue().CallLater(TickCallQueue, 100, true);
		m_CallQueue.CallLater(PollDestruction, (int)(m_fPollInterval * 1000), true);

		// Spawn the first vehicle after a short delay to let the world finish loading
		m_bPendingSpawn = true;
		m_CallQueue.CallLater(SpawnReplacement, 2000, false);

		if (m_bDebugLog)
			Print("[INS_VehicleRespawner] Initialised at " + owner.GetName() + ", first spawn in 2s.");
	}

	override void OnDelete(IEntity owner)
	{
		// Only fires when the level unloads, the spawn point is never GC'd during gameplay
		GetGame().GetCallqueue().Remove(TickCallQueue);
		super.OnDelete(owner);
	}

	protected void TickCallQueue()
	{
		m_CallQueue.Tick(0.1);
	}

	protected void PollDestruction()
	{
		// Waiting for the delay to expire, don't double-trigger
		if (m_bPendingSpawn)
			return;

		if (!m_CurrentVehicle)
		{
			// Vehicle entity went null, either GC'd or deleted by the damage system.
			// This is the normal path for aircraft that crash somewhere remote.
			if (m_iMaxRespawns >= 0 && m_iRespawnCount >= m_iMaxRespawns)
			{
				if (m_bDebugLog)
					Print("[INS_VehicleRespawner] Max respawns reached.");
				m_CallQueue.Remove(PollDestruction);
				return;
			}

			float delay = GetActiveDelay();
			m_bPendingSpawn = true;
			if (m_bDebugLog)
				Print(string.Format("[INS_VehicleRespawner] Vehicle gone (destroyed or GC'd), replacement in %1s.", (int)delay));
			m_CallQueue.CallLater(SpawnReplacement, (int)(delay * 1000), false);
			return;
		}

		SCR_DamageManagerComponent dmgMgr = SCR_DamageManagerComponent.GetDamageManager(m_CurrentVehicle);
		if (!dmgMgr)
		{
			// No damage manager, treat the same as a null entity, queue a respawn
			if (m_iMaxRespawns >= 0 && m_iRespawnCount >= m_iMaxRespawns)
			{
				if (m_bDebugLog)
					Print("[INS_VehicleRespawner] Max respawns reached.");
				m_CallQueue.Remove(PollDestruction);
				return;
			}

			float delay = GetActiveDelay();
			m_bPendingSpawn  = true;
			m_CurrentVehicle = null;
			if (m_bDebugLog)
				Print(string.Format("[INS_VehicleRespawner] No damage manager found, replacement in %1s.", (int)delay));
			m_CallQueue.CallLater(SpawnReplacement, (int)(delay * 1000), false);
			return;
		}

		if (dmgMgr.GetHealthScaled() > 0)
			return;

		// Vehicle is destroyed
		if (m_iMaxRespawns >= 0 && m_iRespawnCount >= m_iMaxRespawns)
		{
			if (m_bDebugLog)
				Print("[INS_VehicleRespawner] Max respawns reached.");
			m_CallQueue.Remove(PollDestruction);
			return;
		}

		float delay = GetActiveDelay();
		m_bPendingSpawn  = true;
		m_CurrentVehicle = null;
		if (m_bDebugLog)
			Print(string.Format("[INS_VehicleRespawner] Vehicle destroyed, replacement in %1s.", (int)delay));
		m_CallQueue.CallLater(SpawnReplacement, (int)(delay * 1000), false);
	}

	protected void SpawnReplacement()
	{
		// If a healthy vehicle already exists at the spawn point (e.g. restored by
		// persistence after a crash), adopt it rather than spawning a duplicate on top.
		IEntity existing = FindHealthyVehicleNearPoint(m_vSpawnTransform[3], 2.0);
		if (existing)
		{
			m_CurrentVehicle = existing;
			m_bPendingSpawn  = false;
			if (m_bDebugLog)
				Print("[INS_VehicleRespawner] Adopted persisted vehicle at " + existing.GetOrigin().ToString());
			return;
		}

		if (m_sVehiclePrefab.IsEmpty())
		{
			Print("[INS_VehicleRespawner] ERROR: No vehicle prefab configured.");
			m_bPendingSpawn = false;
			return;
		}

		Resource res = Resource.Load(m_sVehiclePrefab);
		if (!res || !res.IsValid())
		{
			Print("[INS_VehicleRespawner] ERROR: Cannot load prefab: " + m_sVehiclePrefab);
			m_bPendingSpawn = false;
			return;
		}

		CleanupWrecksNearPoint(m_vSpawnTransform[3]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform     = m_vSpawnTransform;

		IEntity newVehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!newVehicle)
		{
			Print("[INS_VehicleRespawner] ERROR: SpawnEntityPrefab returned null.");
			m_bPendingSpawn = false;
			return;
		}

		m_iRespawnCount++;
		m_CurrentVehicle = newVehicle;
		m_bPendingSpawn  = false;

		string limit = "unlimited";
		if (m_iMaxRespawns >= 0)
			limit = m_iMaxRespawns.ToString();

		if (m_bDebugLog)
			Print(string.Format("[INS_VehicleRespawner] Vehicle spawned (spawn %1/%2).", m_iRespawnCount, limit));
	}

	protected float GetActiveDelay()
	{
		// Local value takes priority
		if (m_fRespawnDelay > 0)
			return m_fRespawnDelay;

		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header || !header.m_Insurgency)
			return 120;

		switch (m_eVehicleType)
		{
			case EINS_VehicleType.Aircraft: return header.m_Insurgency.m_fAircraftRespawnDelay;
			case EINS_VehicleType.Armour:   return header.m_Insurgency.m_fArmourRespawnDelay;
			default:                        return header.m_Insurgency.m_fWheeledRespawnDelay;
		}

		return header.m_Insurgency.m_fWheeledRespawnDelay;
	}

	// Temp storage for the healthy vehicle search callback, cleared after use
	protected IEntity m_FoundVehicle = null;

	protected IEntity FindHealthyVehicleNearPoint(vector origin, float radius)
	{
		m_FoundVehicle = null;
		GetGame().GetWorld().QueryEntitiesBySphere(origin, radius, HealthyVehicleCallback);
		IEntity result = m_FoundVehicle;
		m_FoundVehicle = null;
		return result;
	}

	protected bool HealthyVehicleCallback(IEntity ent)
	{
		if (!ent || m_FoundVehicle)
			return true;

		if (!Vehicle.Cast(ent))
			return true;

		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.GetDamageManager(ent);
		if (!dmg || dmg.GetHealthScaled() > 0)
			m_FoundVehicle = ent;

		return true;
	}

	// Removes destroyed vehicle wrecks within m_fWreckCleanupRadius of the spawn point.
	// Players and non-vehicle entities are never touched.
	protected void CleanupWrecksNearPoint(vector origin)
	{
		m_aWrecksToDelete = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(origin, m_fWreckCleanupRadius, CollectWreckCallback);

		foreach (IEntity wreck : m_aWrecksToDelete)
		{
			if (m_bDebugLog)
				Print("[INS_VehicleRespawner] Removing wreck: " + wreck.GetName());
			SCR_EntityHelper.DeleteEntityAndChildren(wreck);
		}

		m_aWrecksToDelete = null;
	}

	protected bool CollectWreckCallback(IEntity ent)
	{
		if (!ent)
			return true;

		Vehicle vehicle = Vehicle.Cast(ent);
		if (!vehicle)
			return true;

		SCR_DamageManagerComponent dmgMgr = SCR_DamageManagerComponent.GetDamageManager(ent);
		if (dmgMgr && dmgMgr.GetHealthScaled() <= 0)
			m_aWrecksToDelete.Insert(ent);

		return true;
	}
}
