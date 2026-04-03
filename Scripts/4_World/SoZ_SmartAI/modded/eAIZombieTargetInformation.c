modded class eAIZombieTargetInformation
{
	override float CalculateThreat(eAIBase ai = null, eAITargetInformationState state = null)
	{
		float baseThreat = super.CalculateThreat(ai, state);

		//! Slightly boost threat for nearby calm zombies so they stay tracked longer.
		if (ai && baseThreat > 0.0 && baseThreat < 0.15)
		{
			if (!m_Zombie.IsDamageDestroyed() && m_Zombie.Expansion_IsDanger())
			{
				int level = m_Zombie.Expansion_GetMindState();
				if (level == DayZInfectedConstants.MINDSTATE_CALM)
				{
					float distance = GetDistance(ai, true);
					float awarenessRadius = ai.SoZ_GetAwarenessRadius();
					if (distance < awarenessRadius)
					{
						float boost = ExpansionMath.LinearConversion(0.0, awarenessRadius, distance, 0.05, 0.0);
						baseThreat = baseThreat + boost;
					}
				}
			}
		}

		return baseThreat;
	}

	override float GetMinDistance(eAIBase ai = null, float distance = 0.0)
	{
		if (ai)
		{
			//! v5: When flee state is active, ALWAYS maintain 100m distance
			if (ai.m_SoZ_IsFleeing && GetDistanceSq(ai, true) > 16)
				return 100.0;

			auto settings = SoZSmartAISettings.Get();
			float health = ai.GetHealth01("", "");
			int dangerCount = ai.m_eAI_AcuteDangerTargetCount; // zombies + creatures + custom infected

			//! v2: Outnumbered by creatures — flee regardless of health
			if (dangerCount >= settings.FleeZombieCountThreshold && ai.m_eAI_AcuteDangerPlayerTargetCount == 0 && GetDistanceSq(ai, true) > 16)
				return 100.0;

			//! v2: Low health — flee from even a single creature (only if no player threat)
			if (health > 0.0 && health < settings.LowHealthFleeThreshold && ai.m_eAI_AcuteDangerPlayerTargetCount == 0 && GetDistanceSq(ai, true) > 16)
				return 100.0;

			//! v2: Medium health + multiple creatures — flee
			if (health > 0.0 && health < 0.5 && dangerCount >= 2 && ai.m_eAI_AcuteDangerPlayerTargetCount == 0 && GetDistanceSq(ai, true) > 16)
				return 100.0;
		}

		return super.GetMinDistance(ai, distance);
	}

	override bool ShouldAvoid(eAIBase ai = null, float distance = 0.0)
	{
		if (ai && GetMinDistance(ai, distance) > 0.0)
			return true;

		return super.ShouldAvoid(ai, distance);
	}
};
