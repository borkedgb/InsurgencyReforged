// Scenario properties attributes for INS_GarrisonManager.
// by borked.gb

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_GarrisonEnabledEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_GarrisonManager mgr = INS_GarrisonManager.Cast(m_GameMode.FindComponent(INS_GarrisonManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(mgr.IsEnabled());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_GarrisonManager mgr = INS_GarrisonManager.Cast(m_GameMode.FindComponent(INS_GarrisonManager));
		if (!mgr)
			return;

		mgr.SetEnabled(var.GetBool());
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_GarrisonCacheGroupsEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_GarrisonManager mgr = INS_GarrisonManager.Cast(m_GameMode.FindComponent(INS_GarrisonManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetCacheZoneExtraGroups());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_GarrisonManager mgr = INS_GarrisonManager.Cast(m_GameMode.FindComponent(INS_GarrisonManager));
		if (!mgr)
			return;

		mgr.SetCacheZoneExtraGroups((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 6, 1)));
		return outEntries.Count();
	}
}
