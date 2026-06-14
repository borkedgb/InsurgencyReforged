// Scenario properties attributes for INS_AmbientVehicleManager.
// by borked.gb

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_VehiclesEnabledEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientVehicleManager mgr = INS_AmbientVehicleManager.Cast(m_GameMode.FindComponent(INS_AmbientVehicleManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(mgr.IsEnabled());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientVehicleManager mgr = INS_AmbientVehicleManager.Cast(m_GameMode.FindComponent(INS_AmbientVehicleManager));
		if (!mgr)
			return;

		mgr.SetEnabled(var.GetBool());
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_MaxVehiclesEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_AmbientVehicleManager mgr = INS_AmbientVehicleManager.Cast(m_GameMode.FindComponent(INS_AmbientVehicleManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMaxActiveVehicles());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_AmbientVehicleManager mgr = INS_AmbientVehicleManager.Cast(m_GameMode.FindComponent(INS_AmbientVehicleManager));
		if (!mgr)
			return;

		mgr.SetMaxActiveVehicles((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 10, 1)));
		return outEntries.Count();
	}
}
