/**
 * INS_BodySearchAction
 * by borked.gb
 *
 * A UserAction that players can perform on dead enemy bodies to search for intel.
 * Has a configurable chance of finding intel each search. Each body can only be
 * searched once. Found intel contributes toward revealing cache locations.
 *
 * Add this action and INS_BodySearchedComponent to enemy soldier prefabs via their
 * ActionsManagerComponent and component list respectively.
 *
 * Needs INS_BodySearchedComponent on the same prefab, otherwise searched state
 * only exists server-side.
 */

class INS_BodySearchAction : ScriptedUserAction
{
	protected INS_BodySearchedComponent GetSearchedComp()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		return INS_BodySearchedComponent.Cast(owner.FindComponent(INS_BodySearchedComponent));
	}

	override bool GetActionNameScript(out string outName)
	{
		INS_BodySearchedComponent comp = GetSearchedComp();
		if (comp && comp.IsSearched())
			outName = "Already Searched";
		else
			outName = "Search for Intel";

		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		INS_BodySearchedComponent comp = GetSearchedComp();
		if (comp && comp.IsSearched())
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		CharacterControllerComponent charCtrl = CharacterControllerComponent.Cast(
			owner.FindComponent(CharacterControllerComponent)
		);

		if (!charCtrl)
			return false;

		ECharacterLifeState lifeState = charCtrl.GetLifeState();
		return lifeState != ECharacterLifeState.ALIVE;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		INS_BodySearchedComponent comp = GetSearchedComp();
		return !comp || !comp.IsSearched();
	}

	// PerformAction runs server side. Rolls for intel, delegates hints to IntelManager.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		INS_BodySearchedComponent comp = INS_BodySearchedComponent.Cast(
			pOwnerEntity.FindComponent(INS_BodySearchedComponent)
		);

		if (!comp)
		{
			Print("[INS_BodySearchAction] INS_BodySearchedComponent missing on body - add it to the enemy prefab alongside this action.", LogLevel.WARNING);
			return;
		}

		if (comp.IsSearched())
			return;

		comp.MarkSearched();

		INS_IntelManager intelMgr = INS_IntelManager.GetInstance();

		int searcherID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);

		float dropChance = 0.05;
		if (intelMgr)
			dropChance = intelMgr.GetIntelDropChance();

		float roll = Math.RandomFloat01();
		if (roll <= dropChance)
		{
			if (intelMgr)
				intelMgr.AddIntelAndBroadcast(1, searcherID);
		}
		else
		{
			if (intelMgr)
				intelMgr.BroadcastSearchFailed(searcherID);
		}
	}
}
