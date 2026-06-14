/**
 * INS_CacheDestructionHook
 * by borked.gb
 *
 * Overrides SCR_DestructionMultiPhaseComponent.OnDamageStateChanged to detect
 * when a cache entity (one that has INS_CacheComponent) is fully destroyed.
 * Notifies INS_IntelManager to complete the linked task.
 */

modded class SCR_DestructionMultiPhaseComponent
{
	override void OnDamageStateChanged(EDamageState newState, EDamageState previousDamageState, bool isJIP)
	{
		super.OnDamageStateChanged(newState, previousDamageState, isJIP);

		if (!Replication.IsServer())
			return;

		if (newState != EDamageState.DESTROYED)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Let the intel manager determine if this is a registered cache.
		// It returns immediately if the entity is not in its registry.
		INS_IntelManager mgr = INS_IntelManager.GetInstance();
		if (!mgr)
			return;

		if (!mgr.IsRegisteredCache(owner))
			return;

		mgr.OnCacheDestroyed(owner);
	}
}
