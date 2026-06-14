// Scenario properties attributes for INS_FieldThreatManager.
// by borked.gb

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_FieldThreatEnabledEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(mgr.IsEnabled());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return;

		mgr.SetEnabled(var.GetBool());
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_FieldThreatMaxPatrolsEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMaxConcurrentPatrols());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return;

		mgr.SetMaxConcurrentPatrols((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 10, 1)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_FieldThreatMinCooldownEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMinCooldown());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return;

		mgr.SetMinCooldown(var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 1800, 60)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_FieldThreatMaxCooldownEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMaxCooldown());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_FieldThreatManager mgr = INS_FieldThreatManager.Cast(m_GameMode.FindComponent(INS_FieldThreatManager));
		if (!mgr)
			return;

		mgr.SetMaxCooldown(var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 3600, 60)));
		return outEntries.Count();
	}
}
