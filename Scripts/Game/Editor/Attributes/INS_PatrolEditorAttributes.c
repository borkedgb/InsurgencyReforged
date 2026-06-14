// Scenario properties attributes for INS_AmbientPatrolManager.
// by borked.gb

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_PatrolsEnabledEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(mgr.IsEnabled());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return;

		mgr.SetEnabled(var.GetBool());
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_PatrolRespawnDelayEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetRespawnDelay());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return;

		mgr.SetRespawnDelay(var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 7200, 60)));
		return outEntries.Count();
	}
}

// EMapDescriptorType 59 = Town
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_TownPatrolCountEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetGroupsPerTown());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return;

		mgr.SetGroupsPerTown((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 10, 1)));
		return outEntries.Count();
	}
}

// EMapDescriptorType 60 = Village
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_VillagePatrolCountEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetGroupsPerVillage());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return;

		mgr.SetGroupsPerVillage((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 10, 1)));
		return outEntries.Count();
	}
}

// EMapDescriptorType 61 = Settlement
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_SettlementPatrolCountEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetGroupsPerSettlement());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientPatrolManager mgr = INS_AmbientPatrolManager.Cast(m_GameMode.FindComponent(INS_AmbientPatrolManager));
		if (!mgr)
			return;

		mgr.SetGroupsPerSettlement((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 10, 1)));
		return outEntries.Count();
	}
}
