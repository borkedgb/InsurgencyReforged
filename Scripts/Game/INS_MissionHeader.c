// Mission header params for Insurgency Reforged.
// Bake an m_Insurgency block into InsEden.conf to set per-scenario defaults.
// Server admins can still override any of these via the missionHeader JSON block.
// Omitting the conf block entirely falls back to the auto-initialised defaults below.

[BaseContainerProps()]
class INS_MissionParams
{
	[Attribute("3",    UIWidgets.EditBox, "Number of weapon caches to spawn.")]
	int m_iCacheCount;

	[Attribute("10",   UIWidgets.EditBox, "Intel items required to fully reveal each cache.")]
	int m_iIntelThreshold;

	[Attribute("0.05", UIWidgets.EditBox, "Probability (0.0 - 1.0) that a body yields intel when searched.")]
	float m_fIntelDropChance;

	[Attribute("1",    UIWidgets.CheckBox, "Enable ambient infantry patrols.")]
	bool m_bPatrolsEnabled;

	[Attribute("1",    UIWidgets.EditBox, "Infantry groups per town.")]
	int m_iGroupsPerTown;

	[Attribute("1",    UIWidgets.EditBox, "Infantry groups per village.")]
	int m_iGroupsPerVillage;

	[Attribute("2",    UIWidgets.EditBox, "Infantry groups per settlement.")]
	int m_iGroupsPerSettlement;

	[Attribute("3600", UIWidgets.EditBox, "Seconds before a cleared patrol zone respawns enemies.")]
	float m_fPatrolRespawnDelay;

	[Attribute("1",    UIWidgets.CheckBox, "Enable garrison defenders at town zones.")]
	bool m_bGarrisonsEnabled;

	[Attribute("2",    UIWidgets.EditBox, "Extra infantry groups added at garrison cache zones.")]
	int m_iGarrisonCacheExtraGroups;

	[Attribute("1",    UIWidgets.CheckBox, "Enable enemy vehicle patrols.")]
	bool m_bVehiclesEnabled;

	[Attribute("4",    UIWidgets.EditBox, "Maximum enemy vehicles active at once.")]
	int m_iMaxActiveVehicles;

	[Attribute("1",    UIWidgets.CheckBox, "Enable counter-attack squads.")]
	bool m_bCounterAttacksEnabled;

	[Attribute("2",    UIWidgets.EditBox, "Maximum concurrent counter-attack squads.")]
	int m_iMaxConcurrentCounterAttacks;

	[Attribute("600",  UIWidgets.EditBox, "Seconds between counter-attack attempts.")]
	float m_fCounterAttackInterval;

	[Attribute("1",    UIWidgets.EditBox, "Minimum squads spawned per counter-attack wave.")]
	int m_iMinSquadsPerAttack;

	[Attribute("3",    UIWidgets.EditBox, "Maximum squads spawned per counter-attack wave.")]
	int m_iMaxSquadsPerAttack;

	[Attribute("1200", UIWidgets.EditBox, "Seconds before the same town can be counter-attacked again.")]
	float m_fZoneCooldown;

	[Attribute("1",    UIWidgets.CheckBox, "Enable field threat patrols.")]
	bool m_bFieldThreatEnabled;

	[Attribute("3",    UIWidgets.EditBox, "Maximum field threat patrols active at once.")]
	int m_iMaxConcurrentFieldPatrols;

	[Attribute("600",  UIWidgets.EditBox, "Minimum seconds before a field threat patrol spawns on a player group.")]
	float m_fThreatMinCooldown;

	[Attribute("1200",  UIWidgets.EditBox, "Maximum seconds before a field threat patrol spawns on a player group.")]
	float m_fThreatMaxCooldown;
}

modded class SCR_MissionHeader
{
	[Attribute()]
	ref INS_MissionParams m_Insurgency = new INS_MissionParams();
}

// Kept so any configs that reference INS_MissionHeader by name still compile.
class INS_MissionHeader : SCR_MissionHeader {}
