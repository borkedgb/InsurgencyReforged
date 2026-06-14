/**
 * INS_GarrisonZone
 * by borked.gb
 *
 * Place this component on a GenericEntity in the World Editor to mark it as a
 * garrison zone. INS_GarrisonManager will spawn groups here when players get close.
 *
 * Usage:
 *   - Use m_iGroupCount 3-4 for towns, 1-2 for road junctions and outposts
 *   - Tick m_bIsCacheZone at cache spawn point locations for extra defenders
 *   - Set m_iVehicleCount to spawn vehicle patrols alongside the infantry
 */

[ComponentEditorProps(category: "Insurgency", description: "Marks this entity as a garrison zone. INS_GarrisonManager spawns groups here when players are nearby.")]
class INS_GarrisonZoneClass : ScriptComponentClass {}

class INS_GarrisonZone : ScriptComponent
{
	[Attribute("150", UIWidgets.EditBox, "Radius (m) within which AI groups are scattered on spawn.")]
	protected float m_fRadius;

	[Attribute("2", UIWidgets.EditBox, "Number of infantry groups to spawn in this zone.")]
	protected int m_iGroupCount;

	[Attribute("0", UIWidgets.CheckBox, "Tick this at cache spawn point locations to get extra defender groups.")]
	protected bool m_bIsCacheZone;

	[Attribute("0", UIWidgets.EditBox, "Number of vehicle patrols to spawn in this zone. 0 = none.")]
	protected int m_iVehicleCount;

	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging.")]
	protected bool m_bDebugLog = false;

	protected static ref array<INS_GarrisonZone> s_aZones = {};

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_aZones.Insert(this);
		if (m_bDebugLog)
			Print(string.Format("[INS_GarrisonZone] Registered at %1. Total zones: %2",
				owner.GetOrigin().ToString(), s_aZones.Count()));
	}

	float GetRadius()       { return m_fRadius; }
	int   GetGroupCount()   { return m_iGroupCount; }
	bool  IsCacheZone()     { return m_bIsCacheZone; }
	int   GetVehicleCount() { return m_iVehicleCount; }

	static int CollectZones(out array<INS_GarrisonZone> outZones)
	{
		// The static array persists across Workbench stop/play cycles, so prune any stale refs before returning
		int i = 0;
		while (i < s_aZones.Count())
		{
			INS_GarrisonZone z = s_aZones[i];
			if (!z || !z.GetOwner())
				s_aZones.Remove(i);
			else
				i++;
		}

		outZones.Copy(s_aZones);
		return outZones.Count();
	}
}
