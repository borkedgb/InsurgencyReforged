/**
 * INS_AmbientZone
 * by borked.gb
 *
 * Ambient patrol zone component. Attach to a GenericEntity in the World Editor
 *
 * INS_AmbientPatrolManager picks these up and merges them into its zone list.
 * INS_AmbientVehicleManager picks up zones with m_bIsVehicleNode set as extra route nodes.
 */

[ComponentEditorProps(category: "Insurgency", description: "Manual ambient patrol zone override for INS_AmbientPatrolManager / INS_AmbientVehicleManager.")]
class INS_AmbientZoneClass : ScriptComponentClass {}

class INS_AmbientZone : ScriptComponent
{
	[Attribute("200", UIWidgets.EditBox, "Patrol radius (m).")]
	protected float m_fRadius;

	[Attribute("2", UIWidgets.EditBox, "Number of infantry groups to spawn here.")]
	protected int m_iGroupCount;

	[Attribute("0", UIWidgets.CheckBox, "If set, INS_AmbientVehicleManager will treat this as a vehicle route node.")]
	protected bool m_bIsVehicleNode;

	protected static ref array<INS_AmbientZone> s_aZones = {};

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_aZones.Insert(this);
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);
		int idx = s_aZones.Find(this);
		if (idx >= 0)
			s_aZones.Remove(idx);
	}

	float GetRadius()     { return m_fRadius; }
	int   GetGroupCount() { return m_iGroupCount; }
	bool  IsVehicleNode() { return m_bIsVehicleNode; }

	static int CollectZones(out array<INS_AmbientZone> outZones)
	{
		// Prune stale refs left over in Workbench play sessions
		int i = 0;
		while (i < s_aZones.Count())
		{
			INS_AmbientZone z = s_aZones[i];
			if (!z || !z.GetOwner())
				s_aZones.Remove(i);
			else
				i++;
		}

		outZones.Copy(s_aZones);
		return outZones.Count();
	}
}
