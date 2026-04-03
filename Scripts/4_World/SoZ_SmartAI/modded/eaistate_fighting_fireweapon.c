modded class eAIState_Fighting_FireWeapon
{
	override int Guard()
	{
		auto settings = SoZSmartAISettings.Get();

		if (settings.PreferMeleeVsZombies)
		{
			eAITarget target = unit.GetTarget();
			if (target && target.IsZombie())
			{
				if (unit.m_eAI_AcuteDangerPlayerTargetCount == 0)
				{
					//! Allow shooting if low health — melee swap won't reliably happen
					//! (TakeItemToHands requires Health > 0.8 for silent attack)
					float health = unit.GetHealth01("", "");
					if (health < 0.5)
						return super.Guard();

					float maxDist = settings.MeleePreferenceMaxDistance;
					if (target.GetDistanceSq(true) < maxDist * maxDist)
					{
						PrintToRPT("[SoZ] " + unit.GetDisplayName() + " SUPPRESSED gunfire vs zombie (dist=" + Math.Sqrt(target.GetDistanceSq(true)) + "m) at " + unit.GetPosition());
						return eAITransition.FAIL;
					}
				}
			}
		}

		return super.Guard();
	}
};
