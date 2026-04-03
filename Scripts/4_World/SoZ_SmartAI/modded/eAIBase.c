enum SoZ_DetectionState
{
	NO_THREAT = 0,
	UNDETECTED = 1,
	ZOMBIE_ALERTED = 2,
	PLAYER_CLOSE = 3,
	OVERWHELMED = 4
};

modded class eAIBase
{
	private bool m_SoZ_Logged;
	bool m_SoZ_IsFleeing;
	float m_SoZ_NoiseAlertTimer;
	vector m_SoZ_NoiseAlertDirection;

	// v5: Detection + patrol variety
	int m_SoZ_DetectionState;
	float m_SoZ_PatrolSprintTimer;
	float m_SoZ_PatrolSprintCooldown;

	bool SoZ_HasNearbyZombieTargets(float radius)
	{
		if (!m_SoZ_Logged)
		{
			PrintToRPT("[SoZ] " + GetDisplayName() + " ACTIVE at " + GetPosition() + " (targets:" + TargetCount() + ")");
			m_SoZ_Logged = true;
		}

		float radiusSq = radius * radius;
		int count = TargetCount();

		for (int i = 0; i < count; i++)
		{
			eAITarget target = GetTarget(i);
			if (target && target.IsZombie() && target.GetDistanceSq(true) < radiusSq)
				return true;
		}

		return false;
	}

	eAITarget SoZ_GetNearestZombieTarget(float radius)
	{
		float radiusSq = radius * radius;
		float nearestDistSq = radiusSq;
		eAITarget nearest = null;
		int count = TargetCount();

		for (int i = 0; i < count; i++)
		{
			eAITarget target = GetTarget(i);
			if (target && target.IsZombie())
			{
				float distSq = target.GetDistanceSq(true);
				if (distSq < nearestDistSq)
				{
					nearestDistSq = distSq;
					nearest = target;
				}
			}
		}

		return nearest;
	}

	eAITarget SoZ_GetNearestDangerousZombie(float radius)
	{
		float radiusSq = radius * radius;
		float nearestDistSq = radiusSq;
		eAITarget nearest = null;
		int count = TargetCount();

		for (int i = 0; i < count; i++)
		{
			eAITarget target = GetTarget(i);
			if (!target || !target.IsZombie())
				continue;

			float distSq = target.GetDistanceSq(true);
			if (distSq >= nearestDistSq)
				continue;

			float threat = target.GetThreat();
			if (threat > 0.12)
			{
				nearestDistSq = distSq;
				nearest = target;
			}
		}

		return nearest;
	}

	//! v4: Find nearest enemy player (not in group, not unconscious, not yet in combat)
	eAITarget SoZ_GetNearestEnemyPlayer(float radius)
	{
		float radiusSq = radius * radius;
		float nearestDistSq = radiusSq;
		eAITarget nearest = null;
		int count = TargetCount();

		for (int i = 0; i < count; i++)
		{
			eAITarget target = GetTarget(i);
			if (!target || !target.IsPlayer())
				continue;

			if (target.IsUnconscious())
				continue;

			float threat = target.GetThreat();
			if (threat >= 0.4)
				continue; // Already in combat range — Fighting FSM handles this

			EntityAI entity = target.GetEntity();
			if (!entity || !PlayerIsEnemy(entity))
				continue; // Friendly/neutral

			float distSq = target.GetDistanceSq(true);
			if (distSq < nearestDistSq)
			{
				nearestDistSq = distSq;
				nearest = target;
			}
		}

		return nearest;
	}

	int SoZ_CountNearbyZombies(float radius)
	{
		float radiusSq = radius * radius;
		int zombieCount = 0;
		int count = TargetCount();

		for (int i = 0; i < count; i++)
		{
			eAITarget target = GetTarget(i);
			if (target && target.IsZombie() && target.GetDistanceSq(true) < radiusSq)
				zombieCount++;
		}

		return zombieCount;
	}

	float SoZ_GetAwarenessRadius()
	{
		auto settings = SoZSmartAISettings.Get();

		if (eAI_IsLowVitals())
			return settings.ZombieAwarenessRadiusLowHealth;

		return settings.ZombieAwarenessRadius;
	}

	//! v5: Check if any zombie in ALERTED+ state is within half the awareness radius
	//! Only close alerted zombies trigger this — distant ones investigating other noise don't count
	bool SoZ_HasAlertedZombieNearby(float radius)
	{
		float halfRadius = radius * 0.5;
		float closeRadiusSq = halfRadius * halfRadius;
		int count = TargetCount();

		for (int i = 0; i < count; i++)
		{
			eAITarget target = GetTarget(i);
			if (!target || !target.IsZombie())
				continue;

			if (target.GetDistanceSq(true) >= closeRadiusSq)
				continue;

			EntityAI entity = target.GetEntity();
			ZombieBase zombie;
			if (Class.CastTo(zombie, entity))
			{
				if (zombie.Expansion_GetMindState() >= DayZInfectedConstants.MINDSTATE_ALERTED)
					return true;
			}
		}

		return false;
	}

	//! v5: Evaluate detection state — priority: OVERWHELMED > PLAYER_CLOSE > ZOMBIE_ALERTED
	int SoZ_EvaluateDetectionState()
	{
		auto settings = SoZSmartAISettings.Get();

		if (!settings.EnableDetectionAwareness)
			return SoZ_DetectionState.NO_THREAT;

		// 1. OVERWHELMED (flee active)
		if (m_SoZ_IsFleeing)
			return SoZ_DetectionState.OVERWHELMED;

		// 2. PLAYER_CLOSE (priority over zombie — close player = definitely spotted)
		if (settings.EnablePlayerStealth)
		{
			eAITarget closePlayer = SoZ_GetNearestEnemyPlayer(settings.PlayerCloseDetectionRadius);
			if (closePlayer)
				return SoZ_DetectionState.PLAYER_CLOSE;
		}

		// 3. ZOMBIE_ALERTED (nearby alerted zombie investigating)
		float radius = SoZ_GetAwarenessRadius();
		if (SoZ_HasAlertedZombieNearby(radius))
			return SoZ_DetectionState.ZOMBIE_ALERTED;

		// Caller checks separately for UNDETECTED (threats present but calm/distant)
		return SoZ_DetectionState.NO_THREAT;
	}

	//! v5: Patrol speed variety — occasional sprint bursts, respects patrol config base speed
	int SoZ_GetPatrolSpeed(float dt)
	{
		auto settings = SoZSmartAISettings.Get();
		int baseSpeed = m_eAI_SpeedLimitPreference;

		if (!settings.EnablePatrolSpeedVariety || baseSpeed < 2)
			return baseSpeed; // Don't upgrade WALK patrols

		// Active sprint burst
		if (m_SoZ_PatrolSprintTimer > 0)
		{
			m_SoZ_PatrolSprintTimer -= dt;
			return 3;
		}

		// Cooldown between rolls
		if (m_SoZ_PatrolSprintCooldown > 0)
		{
			m_SoZ_PatrolSprintCooldown -= dt;
			return baseSpeed;
		}

		// Roll for sprint
		if (Math.RandomFloat01() < settings.PatrolSprintChance)
		{
			m_SoZ_PatrolSprintTimer = Math.RandomFloat(settings.PatrolSprintDurationMin, settings.PatrolSprintDurationMax);
			return 3;
		}

		m_SoZ_PatrolSprintCooldown = 3.0;
		return baseSpeed;
	}

	void SoZ_UpdateFleeState()
	{
		auto settings = SoZSmartAISettings.Get();
		int dangerCount = m_eAI_AcuteDangerTargetCount;
		float health = GetHealth01("", "");
		bool shouldFlee = false;

		if (m_eAI_AcuteDangerPlayerTargetCount == 0)
		{
			if (dangerCount >= settings.FleeZombieCountThreshold)
				shouldFlee = true;
			else if (health > 0.0 && health < settings.LowHealthFleeThreshold)
				shouldFlee = true;
			else if (health > 0.0 && health < 0.5 && dangerCount >= 2)
				shouldFlee = true;
		}

		if (shouldFlee && !m_SoZ_IsFleeing)
		{
			PrintToRPT("[SoZ] " + GetDisplayName() + " FLEE (dangers:" + dangerCount + " health:" + (health * 100) + "%) at " + GetPosition());
			m_SoZ_IsFleeing = true;
		}
		else if (!shouldFlee && m_SoZ_IsFleeing)
		{
			PrintToRPT("[SoZ] " + GetDisplayName() + " STOP flee at " + GetPosition());
			m_SoZ_IsFleeing = false;
		}
	}

	//! v4: Noise hook — duck on enemy gunfire/explosions
	override void eAI_OnNoiseEvent(EntityAI source, vector position, float lifetime, eAINoiseParams params, float strengthMultiplier)
	{
		super.eAI_OnNoiseEvent(source, position, lifetime, params, strengthMultiplier);

		if (!SoZSmartAISettings.Get().EnableNoiseAlert)
			return;

		// Respect Expansion's noise investigation config
		if (m_eAI_NoiseInvestigationDistanceLimit <= 0)
			return;

		if (params.m_Type != eAINoiseType.SHOT && params.m_Type != eAINoiseType.EXPLOSION)
			return;

		// Hierarchy root = the player/AI, not the weapon
		EntityAI root = source;
		if (source)
			root = EntityAI.Cast(source.GetHierarchyRoot());

		// Ignore own shots
		if (root == this)
			return;

		// Fix vector.Zero position
		vector noisePos = position;
		if (noisePos == vector.Zero && root)
			noisePos = root.GetPosition();

		// Range check
		float strength = params.m_Strength * strengthMultiplier;
		float distSq = vector.DistanceSq(GetPosition(), noisePos);
		if (distSq > strength * strength)
			return;

		// Friendly shots — ignore
		PlayerBase srcPlayer;
		if (root && Class.CastTo(srcPlayer, root))
		{
			// Already have LOS to shooter — no noise alert needed
			eAITargetInformation srcInfo = srcPlayer.GetTargetInformation();
			if (srcInfo && eAI_HasLOS(srcInfo))
				return;

			// Friendly — ignore
			if (!PlayerIsEnemy(srcPlayer))
				return;
		}

		// Set alert timer
		auto settings = SoZSmartAISettings.Get();
		m_SoZ_NoiseAlertTimer = settings.NoiseAlertDuration;
		m_SoZ_NoiseAlertDirection = noisePos;
		PrintToRPT("[SoZ] " + GetDisplayName() + " NOISE ALERT (shot/explosion) at " + GetPosition() + " → dir " + noisePos);
	}
};
