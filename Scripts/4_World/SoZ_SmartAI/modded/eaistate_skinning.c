modded class eAIState_Skinning
{
	override int Guard()
	{
		// Let base check first: corpse nearby, has knife, wants food, etc.
		int baseResult = super.Guard();
		if (baseResult != eAITransition.SUCCESS)
			return baseResult;

		// Base wants to skin — now check if it's safe
		auto settings = SoZSmartAISettings.Get();

		if (unit.SoZ_HasNearbyZombieTargets(settings.SafeLootingRadius))
		{
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " BLOCKED looting (zombies nearby) at " + unit.GetPosition());
			return eAITransition.FAIL;
		}

		if (unit.SoZ_GetNearestEnemyPlayer(settings.SafeLootingRadius))
		{
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " BLOCKED looting (player nearby) at " + unit.GetPosition());
			return eAITransition.FAIL;
		}

		return eAITransition.SUCCESS;
	}

	override int OnUpdate(float DeltaTime, int SimulationPrecision)
	{
		auto settings = SoZSmartAISettings.Get();

		// Use GetThreatToSelf() instead of filtered helper — catches ALL threats including combat-level players
		// (SoZ_GetNearestEnemyPlayer excludes threat>=0.4, but during skinning Fighting can't take over)
		bool zombiesNear = unit.SoZ_HasNearbyZombieTargets(settings.SafeLootingRadius);
		bool threatened = unit.GetThreatToSelf() > 0.2;
		bool danger = zombiesNear || threatened;

		if (danger)
		{
			// Cancel running action then exit state
			ActionManagerBase actionMgr = unit.GetActionManager();
			if (actionMgr && actionMgr.GetRunningAction())
				actionMgr.Interrupt();

			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " ABORT looting (danger appeared) at " + unit.GetPosition());
			return EXIT;
		}

		return super.OnUpdate(DeltaTime, SimulationPrecision);
	}
};
