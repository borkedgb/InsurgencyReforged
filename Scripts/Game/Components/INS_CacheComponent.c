/**
 * INS_CacheComponent
 * by borked.gb
 *
 * Attach this to the cache prefab entity. Tags it so INS_CacheSpawnManager can
 * identify and adopt a persisted cache on crash recovery rather than spawning a
 * fresh one on top. Registration with INS_IntelManager is handled exclusively by
 * INS_CacheSpawnManager so the order matches cache_positions.dat and intel_state.dat.
 *
 * Destruction detection is handled by INS_CacheDestructionHook
 * (modded SCR_DestructionMultiPhaseComponent).
 */

[ComponentEditorProps(category: "Insurgency", description: "Marks this entity as a weapons cache. Required for crash-recovery adoption by INS_CacheSpawnManager.")]
class INS_CacheComponentClass : ScriptComponentClass
{
}

class INS_CacheComponent : ScriptComponent
{
}
