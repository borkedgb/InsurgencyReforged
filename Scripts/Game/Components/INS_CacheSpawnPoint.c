/**
 * INS_CacheSpawnPoint
 * by borked.gb
 *
 * A marker component. Place generic entities with this component inthe World Editor
 * to define where weapons caches can randomly spawn. The INS_CacheSpawnManager
 * picks a random subset of these at game start.
 *
 * Uses a static registry so registration is not dependent on GameMode init order.
 */

[ComponentEditorProps(category: "Insurgency", description: "Marks this entity as a potential cache spawn location. Place in towns.")]
class INS_CacheSpawnPointClass : ScriptComponentClass
{
}

class INS_CacheSpawnPoint : ScriptComponent
{
	protected static ref array<INS_CacheSpawnPoint> s_aInstances = new array<INS_CacheSpawnPoint>();

	// Returns world positions of all registered spawn points
	static int CollectPositions(out notnull array<vector> outPositions)
	{
		outPositions.Clear();
		foreach (INS_CacheSpawnPoint sp : s_aInstances)
		{
			if (sp && sp.GetOwner())
				outPositions.Insert(sp.GetOwner().GetOrigin());
		}
		return outPositions.Count();
	}

	// Register into static list on init, no dependency on GameMode being ready
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_aInstances.Insert(this);
		Print("[INS_CacheSpawnPoint] Registered at: " + owner.GetOrigin().ToString());
	}

	override void OnDelete(IEntity owner)
	{
		s_aInstances.Remove(s_aInstances.Find(this));
		super.OnDelete(owner);
	}
}
