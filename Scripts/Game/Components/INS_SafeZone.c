/**
 * INS_SafeZone
 * by borked.gb
 *
 * Place on a GenericEntity at the base or spawn area. INS_FieldThreatManager
 * will never spawn a stalker patrol inside this radius.
 */

[ComponentEditorProps(category: "Insurgency", description: "Marks a safe area where INS_FieldThreatManager will not spawn stalker patrols.")]
class INS_SafeZoneClass : ScriptComponentClass {}

class INS_SafeZone : ScriptComponent
{
	[Attribute("300", UIWidgets.EditBox, "Radius (m). No field threat patrols will spawn inside this.")]
	protected float m_fRadius;

	protected static ref array<INS_SafeZone> s_aZones = {};

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

	float GetRadius() { return m_fRadius; }

	static int CollectZones(out array<INS_SafeZone> outZones)
	{
		int i = 0;
		while (i < s_aZones.Count())
		{
			INS_SafeZone z = s_aZones[i];
			if (!z || !z.GetOwner())
				s_aZones.Remove(i);
			else
				i++;
		}

		outZones.Copy(s_aZones);
		return outZones.Count();
	}
}
