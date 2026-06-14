// Scenario properties attributes for INS_CounterAttackManager.
// by borked.gb

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_CounterAttacksEnabledEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(mgr.IsEnabled());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return;

		mgr.SetEnabled(var.GetBool());
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_MaxConcurrentCounterAttacksEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMaxConcurrentCounterAttacks());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return;

		mgr.SetMaxConcurrentCounterAttacks((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 10, 1)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_CounterAttackIntervalEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetCounterAttackInterval());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return;

		mgr.SetCounterAttackInterval(var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(60, 1800, 60)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_MinSquadsPerAttackEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMinSquadsPerAttack());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return;

		mgr.SetMinSquadsPerAttack((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(1, 5, 1)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_MaxSquadsPerAttackEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetMaxSquadsPerAttack());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return;

		mgr.SetMaxSquadsPerAttack((int)var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(1, 5, 1)));
		return outEntries.Count();
	}
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class INS_ZoneCooldownEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!IsGameMode(item))
			return null;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(mgr.GetZoneCooldown());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var,
		SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var || !IsGameMode(item))
			return;

		INS_CounterAttackManager mgr = INS_CounterAttackManager.Cast(m_GameMode.FindComponent(INS_CounterAttackManager));
		if (!mgr)
			return;

		mgr.SetZoneCooldown(var.GetFloat());
	}

	override int GetEntries(notnull array<ref SCR_BaseEditorAttributeEntry> outEntries)
	{
		outEntries.Insert(new SCR_BaseEditorAttributeEntrySlider(new INS_SliderValues(0, 3600, 60)));
		return outEntries.Count();
	}
}
