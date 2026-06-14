/**
 * INS_BodySearchedComponent
 * by borked.gb
 *
 * Add to enemy soldier base prefabs alongside INS_BodySearchAction. Tracks whether
 * a body has been searched. Replicated, so a second player nearby won't keep
 * seeing "Search for Intel" on a body someone else already searched.
 */

[ComponentEditorProps(category: "Insurgency", description: "Tracks whether this body has been searched for intel. Goes alongside INS_BodySearchAction on the enemy soldier prefab.")]
class INS_BodySearchedComponentClass : ScriptComponentClass
{
}

class INS_BodySearchedComponent : ScriptComponent
{
	[RplProp()]
	protected bool m_bSearched = false;

	bool IsSearched() { return m_bSearched; }

	void MarkSearched()
	{
		m_bSearched = true;
		Replication.BumpMe();
	}
}
