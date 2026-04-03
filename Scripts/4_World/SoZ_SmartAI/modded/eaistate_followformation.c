modded class eAIState_FollowFormation
{
	private float m_SoZ_StealthCooldown;
	private bool m_SoZ_InStealth;
	private bool m_SoZ_IsAvoiding;
	private float m_SoZ_AvoidRecalcTimer;
	private vector m_SoZ_AvoidTarget;
	private int m_SoZ_StealthReason;

	// v5: Detection state
	private float m_SoZ_DetectedSprintTimer;
	private int m_SoZ_DetectedSprintSpeed;
	private int m_SoZ_PrevDetectionState;

	override void OnExit(string Event, bool Aborted, ExpansionState To)
	{
		super.OnExit(Event, Aborted, To);

		if (m_SoZ_InStealth && unit.GetThreatToSelf() < 0.4)
		{
			unit.Expansion_GetUp(true);
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT formation-stealth (leaving state) at " + unit.GetPosition());
		}
		m_SoZ_StealthCooldown = 0;
		m_SoZ_InStealth = false;
		m_SoZ_IsAvoiding = false;
		m_SoZ_AvoidRecalcTimer = 0;
		m_SoZ_StealthReason = 0;
		m_SoZ_DetectedSprintTimer = 0;
		m_SoZ_DetectedSprintSpeed = 0;
		m_SoZ_PrevDetectionState = 0;
	}

	override int OnUpdate(float DeltaTime, int SimulationPrecision)
	{
		int result = super.OnUpdate(DeltaTime, SimulationPrecision);

		auto settings = SoZSmartAISettings.Get();
		if (!m_Group)
			return result;

		// ALWAYS update flee state (eAIZombieTargetInformation.GetMinDistance depends on it)
		unit.SoZ_UpdateFleeState();

		if (!settings.EnableStealthCrouch && !settings.EnableDetectionAwareness)
			return result;

		// --- Step 2: Combat handoff ---
		bool threatened = unit.GetThreatToSelf() >= 0.4;
		if (threatened)
		{
			if (m_SoZ_InStealth)
			{
				unit.Expansion_GetUp(true);
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT formation-stealth (combat) at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_IsAvoiding = false;
				m_SoZ_StealthReason = 0;
			}
			m_SoZ_DetectedSprintTimer = 0;
			m_SoZ_DetectedSprintSpeed = 0;
			m_SoZ_PrevDetectionState = 0;
			unit.m_SoZ_PatrolSprintTimer = 0;
			unit.m_SoZ_PatrolSprintCooldown = 0;
			return result;
		}

		// Catch-up check: if not in formation position, stand up and catch up
		float distToFormPos = vector.DistanceSq(unit.GetPosition(), m_Group.GetFormationPosition(unit));
		bool inFormation = distToFormPos < 9.0;

		if (!inFormation)
		{
			if (m_SoZ_InStealth)
			{
				unit.Expansion_GetUp(true);
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT formation-stealth (catch-up) at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_IsAvoiding = false;
				m_SoZ_StealthReason = 0;
			}
			m_SoZ_DetectedSprintTimer = 0;
			m_SoZ_DetectedSprintSpeed = 0;
			unit.m_SoZ_PatrolSprintTimer = 0;
			unit.m_SoZ_PatrolSprintCooldown = 0;
			return result;
		}

		// --- Step 3: Sticky sprint (AFTER flee+threatened+catch-up) ---
		if (m_SoZ_DetectedSprintTimer > 0)
		{
			m_SoZ_DetectedSprintTimer -= DeltaTime;
			unit.Expansion_GetUp(true);
			unit.SetMovementSpeedLimit(m_SoZ_DetectedSprintSpeed);
			if (m_SoZ_IsAvoiding && m_SoZ_AvoidTarget != vector.Zero)
				unit.OverrideTargetPosition(m_SoZ_AvoidTarget, false);
			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
			return result;
		}
		m_SoZ_DetectedSprintSpeed = 0;

		// --- Step 4: Detection state + transition reset ---
		int detState = unit.SoZ_EvaluateDetectionState();

		// Restore to formation catch-up speed (3) when leaving an override state
		bool wasOverride = (m_SoZ_PrevDetectionState >= SoZ_DetectionState.ZOMBIE_ALERTED);
		bool isOverride = (detState >= SoZ_DetectionState.ZOMBIE_ALERTED);
		if (wasOverride && !isOverride)
		{
			unit.SetMovementSpeedLimit(3);
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " RESTORE formation speed (was override) at " + unit.GetPosition());
		}
		m_SoZ_PrevDetectionState = detState;

		// --- Step 5: Stealth triggers (for UNDETECTED case) ---
		float radius = unit.SoZ_GetAwarenessRadius();
		eAITarget dangerZombie = unit.SoZ_GetNearestDangerousZombie(radius);
		eAITarget enemyPlayer = null;
		bool noiseAlert = false;

		if (settings.EnablePlayerStealth)
			enemyPlayer = unit.SoZ_GetNearestEnemyPlayer(settings.PlayerAwarenessRadius);

		if (settings.EnableNoiseAlert && unit.m_SoZ_NoiseAlertTimer > 0)
			noiseAlert = true;

		bool hasThreats = dangerZombie != null || enemyPlayer != null || noiseAlert;

		// Reset patrol sprint timers when not in NO_THREAT without threats
		if (detState != SoZ_DetectionState.NO_THREAT || hasThreats)
		{
			unit.m_SoZ_PatrolSprintTimer = 0;
			unit.m_SoZ_PatrolSprintCooldown = 0;
		}

		// --- Step 6: Behavior ---
		if (detState == SoZ_DetectionState.OVERWHELMED)
		{
			// FLEE SPRINT
			unit.Expansion_GetUp(true);
			unit.SetMovementSpeedLimit(settings.FleeSpeedLimit);
			m_SoZ_DetectedSprintTimer = settings.DetectedSprintDuration;
			m_SoZ_DetectedSprintSpeed = settings.FleeSpeedLimit;
			m_SoZ_AvoidRecalcTimer = 0; // Force immediate avoidance recalc

			if (m_SoZ_InStealth)
			{
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_StealthReason = 0;
			}
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " FLEE SPRINT (formation) at " + unit.GetPosition());

			// Avoidance: offset formation position away from threat
			eAITarget fleeTarget = dangerZombie;
			if (!fleeTarget) fleeTarget = enemyPlayer;
			if (fleeTarget)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector formPos = m_Group.GetFormationPosition(unit);
					vector targetPos = fleeTarget.GetPosition(true);
					vector awayFromTarget = vector.Direction(targetPos, formPos);
					awayFromTarget[1] = 0;
					awayFromTarget.Normalize();
					m_SoZ_AvoidTarget = formPos + awayFromTarget * 10.0;
					m_SoZ_AvoidRecalcTimer = 3.0;
					m_SoZ_IsAvoiding = true;
				}
				unit.OverrideTargetPosition(m_SoZ_AvoidTarget, false);
			}

			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else if (detState == SoZ_DetectionState.PLAYER_CLOSE)
		{
			// PLAYER CLOSE — sprint away
			unit.Expansion_GetUp(true);
			unit.SetMovementSpeedLimit(settings.DetectedByPlayerSpeedLimit);
			m_SoZ_DetectedSprintTimer = settings.DetectedSprintDuration;
			m_SoZ_DetectedSprintSpeed = settings.DetectedByPlayerSpeedLimit;
			m_SoZ_AvoidRecalcTimer = 0; // Force immediate avoidance recalc

			if (m_SoZ_InStealth)
			{
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_StealthReason = 0;
			}
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " PLAYER CLOSE SPRINT (formation) at " + unit.GetPosition());

			if (enemyPlayer)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector formPos2 = m_Group.GetFormationPosition(unit);
					vector playerPos = enemyPlayer.GetPosition(true);
					vector awayFromPlayer = vector.Direction(playerPos, formPos2);
					awayFromPlayer[1] = 0;
					awayFromPlayer.Normalize();
					m_SoZ_AvoidTarget = formPos2 + awayFromPlayer * 10.0;
					m_SoZ_AvoidRecalcTimer = 3.0;
					m_SoZ_IsAvoiding = true;
				}
				unit.OverrideTargetPosition(m_SoZ_AvoidTarget, false);
				unit.LookAtPosition(enemyPlayer.GetPosition(true), true);
			}

			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else if (detState == SoZ_DetectionState.ZOMBIE_ALERTED)
		{
			// ZOMBIE ALERTED — jog away
			unit.Expansion_GetUp(true);
			unit.SetMovementSpeedLimit(settings.ZombieAlertedSpeedLimit);
			m_SoZ_AvoidRecalcTimer = 0; // Force immediate avoidance recalc

			if (m_SoZ_InStealth)
			{
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_StealthReason = 0;
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT formation-stealth → ZOMBIE ALERTED JOG at " + unit.GetPosition());
			}

			if (dangerZombie)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector formPos3 = m_Group.GetFormationPosition(unit);
					vector zombiePos = dangerZombie.GetPosition(true);
					vector awayFromZombie = vector.Direction(zombiePos, formPos3);
					awayFromZombie[1] = 0;
					awayFromZombie.Normalize();
					m_SoZ_AvoidTarget = formPos3 + awayFromZombie * 10.0;
					m_SoZ_AvoidRecalcTimer = 3.0;
					m_SoZ_IsAvoiding = true;
				}
				unit.OverrideTargetPosition(m_SoZ_AvoidTarget, false);
			}

			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else if (hasThreats)
		{
			// UNDETECTED: v4 stealth — crouch + walk + avoidance
			eAITarget avoidTarget = null;
			int reason = 0;
			if (dangerZombie) { avoidTarget = dangerZombie; reason = 1; }
			else if (enemyPlayer) { avoidTarget = enemyPlayer; reason = 2; }
			else { reason = 3; }

			if (!m_SoZ_InStealth || reason != m_SoZ_StealthReason)
			{
				string reasonStr;
				if (reason == 1) reasonStr = "zombie nearby";
				else if (reason == 2) reasonStr = "player nearby";
				else reasonStr = "noise alert";
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " ENTER formation-stealth (" + reasonStr + ") at " + unit.GetPosition());
				m_SoZ_InStealth = true;
				m_SoZ_StealthReason = reason;
				m_SoZ_IsAvoiding = false;
				m_SoZ_AvoidRecalcTimer = 0;
			}

			if (settings.EnableStealthCrouch)
				unit.OverrideStance(DayZPlayerConstants.STANCEIDX_CROUCH, false);
			unit.SetMovementSpeedLimit(settings.StealthSpeedLimit);
			if (reason != 3)
				m_SoZ_StealthCooldown = settings.StealthCooldownSeconds;

			// Avoidance: offset formation position away from threat
			if (avoidTarget)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector formPos4 = m_Group.GetFormationPosition(unit);
					vector targetPos4 = avoidTarget.GetPosition(true);
					vector awayFromTarget4 = vector.Direction(targetPos4, formPos4);
					awayFromTarget4[1] = 0;
					awayFromTarget4.Normalize();

					m_SoZ_AvoidTarget = formPos4 + awayFromTarget4 * 10.0;
					m_SoZ_AvoidRecalcTimer = 3.0;

					if (!m_SoZ_IsAvoiding)
					{
						PrintToRPT("[SoZ] " + unit.GetDisplayName() + " AVOID target (formation) at " + unit.GetPosition());
						m_SoZ_IsAvoiding = true;
					}
				}
				unit.OverrideTargetPosition(m_SoZ_AvoidTarget, false);

				if (reason == 2)
					unit.LookAtPosition(avoidTarget.GetPosition(true), true);
			}
			else if (noiseAlert)
			{
				unit.LookAtPosition(unit.m_SoZ_NoiseAlertDirection, true);
			}

			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else if (m_SoZ_InStealth)
		{
			// Stealth cooldown
			m_SoZ_StealthCooldown -= DeltaTime;
			if (m_SoZ_IsAvoiding)
			{
				m_SoZ_IsAvoiding = false;
				m_SoZ_AvoidRecalcTimer = 0;
			}

			if (m_SoZ_StealthCooldown > 0)
			{
				if (settings.EnableStealthCrouch)
					unit.OverrideStance(DayZPlayerConstants.STANCEIDX_CROUCH, false);
				unit.SetMovementSpeedLimit(settings.StealthSpeedLimit);
			}
			else
			{
				unit.Expansion_GetUp(true);
				unit.SetMovementSpeedLimit(3); // Restore formation catch-up speed
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT formation-stealth (clear) at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthReason = 0;
			}

			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else
		{
			// NO_THREAT — no patrol variety for followers, tick noise timer
			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}

		return result;
	}
};
