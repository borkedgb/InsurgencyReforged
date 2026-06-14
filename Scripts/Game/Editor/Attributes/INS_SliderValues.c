// Helper for constructing slider range data in script.
// by borked.gb
// SCR_EditorAttributeBaseValues has protected fields so we need a subclass to set them.
class INS_SliderValues : SCR_EditorAttributeBaseValues
{
	void INS_SliderValues(float min, float max, float step, int decimals = 0)
	{
		m_fMin                  = min;
		m_fMax                  = max;
		m_fStep                 = step;
		m_iDecimals             = decimals;
		m_sSliderValueFormating = "%1";
	}
}
