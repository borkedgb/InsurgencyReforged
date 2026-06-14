// Scenario properties attributes for INS_CacheSpawnManager and INS_IntelManager.
// by borked.gb

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_IntelThresholdEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_IntelManager mgr = INS_IntelManager.Cast(m_GameMode.FindComponent(INS_IntelManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetIntelThreshold());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_IntelManager mgr = INS_IntelManager.Cast(m_GameMode.FindComponent(INS_IntelManager));
		if (!mgr)
			return;

		mgr.SetIntelThreshold((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(1, 30, 1)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_DebugCacheMarkersEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_IntelManager mgr = INS_IntelManager.Cast(m_GameMode.FindComponent(INS_IntelManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(mgr.GetDebugCacheMarkers());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_IntelManager mgr = INS_IntelManager.Cast(m_GameMode.FindComponent(INS_IntelManager));
		if (!mgr)
			return;

		mgr.SetDebugCacheMarkers(var.GetBool());
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_IntelDropChanceEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_IntelManager mgr = INS_IntelManager.Cast(m_GameMode.FindComponent(INS_IntelManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetIntelDropChance() * 100);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_IntelManager mgr = INS_IntelManager.Cast(m_GameMode.FindComponent(INS_IntelManager));
		if (!mgr)
			return;

		mgr.SetIntelDropChance(var.GetFloat() / 100.0);
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 100, 0.1, 1)));
		return outEntries.Count();
	}
}
