/**
 * INS_ZoneHelpers
 * by borked.gb
 *
 * Static helpers shared across zone managers.
 * Centralised here to stop the copies drifting apart.
 */

class INS_ZoneHelpers
{
	// Distance from 'pos' to the nearest point in 'positions'.
	// Returns 'fallback' when the array is empty.
	static float NearestDist(vector pos, array<vector> positions, float fallback)
	{
		if (positions.IsEmpty())
			return fallback;

		float nearestSq = fallback * fallback;
		foreach (vector p : positions)
		{
			float dSq = vector.DistanceSq(pos, p);
			if (dSq < nearestSq)
				nearestSq = dSq;
		}
		return Math.Sqrt(nearestSq);
	}

	// Random land position within 'radius' of 'center', skipping ocean tiles.
	// Falls back to the zone centre on surface if all attempts land in water.
	static vector RandomPositionInRadius(vector center, float radius)
	{
		BaseWorld world = GetGame().GetWorld();
		float     oceanY = world.GetOceanBaseHeight();

		for (int i = 0; i < 16; i++)
		{
			float angle = Math.RandomFloat01() * 6.2832;
			float dist  = Math.RandomFloat01() * radius;
			float x     = center[0] + dist * Math.Cos(angle);
			float z     = center[2] + dist * Math.Sin(angle);
			float y     = world.GetSurfaceY(x, z);

			if (y <= oceanY + 1.0)
				continue;

			return Vector(x, y, z);
		}

		float cy = world.GetSurfaceY(center[0], center[2]);
		return Vector(center[0], cy, center[2]);
	}

	// True if 'locName' contains keywords that mark a location as non-civilian.
	// Name check is a fallback because location type integers set by terrain designers
	// are unreliable in practice.
	static bool IsExcludedName(string locName)
	{
		if (locName.IsEmpty())
			return false;

		string lower = locName;
		lower.ToLower();

		return lower.IndexOf("military") != -1 || lower.IndexOf("airport") != -1;
	}
}
