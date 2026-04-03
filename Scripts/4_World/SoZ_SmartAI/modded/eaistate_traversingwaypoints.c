modded class eAIState_TraversingWaypoints
{
	private float m_SoZ_StealthCooldown;
	private bool m_SoZ_InStealth;
	private bool m_SoZ_IsAvoiding;
	private float m_SoZ_AvoidRecalcTimer;
	private vector m_SoZ_AvoidTarget;
	private int m_SoZ_StealthReason; // 0=none, 1=zombie, 2=player, 3=noise

	// v5: Detection state
	private float m_SoZ_DetectedSprintTimer;
	private int m_SoZ_DetectedSprintSpeed;
	private int m_SoZ_PrevDetectionState;
	private bool m_SoZ_WasPatrolSprinting;

	override void OnEntry(string Event, ExpansionState From)
	{
		super.OnEntry(Event, From);
	}

	override void OnExit(string Event, bool Aborted, ExpansionState To)
	{
		super.OnExit(Event, Aborted, To);

		if (m_SoZ_InStealth && unit.GetThreatToSelf() < 0.4)
		{
			unit.Expansion_GetUp(true);
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT stealth (leaving patrol) at " + unit.GetPosition());
		}
		m_SoZ_StealthCooldown = 0;
		m_SoZ_InStealth = false;
		m_SoZ_IsAvoiding = false;
		m_SoZ_AvoidRecalcTimer = 0;
		m_SoZ_StealthReason = 0;
		m_SoZ_DetectedSprintTimer = 0;
		m_SoZ_DetectedSprintSpeed = 0;
		m_SoZ_PrevDetectionState = 0;
		m_SoZ_WasPatrolSprinting = false;
	}

	override int OnUpdate(float DeltaTime, int SimulationPrecision)
	{
		int result = super.OnUpdate(DeltaTime, SimulationPrecision);

		auto settings = SoZSmartAISettings.Get();

		// ALWAYS update flee state (eAIZombieTargetInformation.GetMinDistance depends on it)
		unit.SoZ_UpdateFleeState();

		if (!settings.EnableStealthCrouch && !settings.EnableDetectionAwareness && !settings.EnablePatrolSpeedVariety)
			return result;

		// --- Step 2: Combat handoff — Fighting FSM takes over at >= 0.4 ---
		bool threatened = unit.GetThreatToSelf() >= 0.4;
		if (threatened)
		{
			if (m_SoZ_InStealth)
			{
				unit.Expansion_GetUp(true);
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT stealth (combat) at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_IsAvoiding = false;
				m_SoZ_StealthReason = 0;
			}
			m_SoZ_DetectedSprintTimer = 0;
			m_SoZ_DetectedSprintSpeed = 0;
			m_SoZ_PrevDetectionState = 0;
			m_SoZ_WasPatrolSprinting = false;
			unit.m_SoZ_PatrolSprintTimer = 0;
			unit.m_SoZ_PatrolSprintCooldown = 0;
			return result;
		}

		// --- Step 3: Sticky sprint (AFTER flee+threatened) ---
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

		// Group-safe restore ONLY when leaving an override state
		bool wasOverride = (m_SoZ_PrevDetectionState >= SoZ_DetectionState.ZOMBIE_ALERTED);
		bool isOverride = (detState >= SoZ_DetectionState.ZOMBIE_ALERTED);
		if (wasOverride && !isOverride)
		{
			unit.SetMovementSpeedLimit(1);
			unit.eAI_SetBestSpeedLimit(unit.m_eAI_SpeedLimitPreference);
			PrintToRPT("[SoZ] " + unit.GetDisplayName() + " RESTORE speed (was override) at " + unit.GetPosition());
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

		// --- Step 6: Behavior based on detection state ---
		// Reset patrol sprint timers when leaving NO_THREAT
		if (detState != SoZ_DetectionState.NO_THREAT || hasThreats)
		{
			unit.m_SoZ_PatrolSprintTimer = 0;
			unit.m_SoZ_PatrolSprintCooldown = 0;
			m_SoZ_WasPatrolSprinting = false;
		}

		if (detState == SoZ_DetectionState.OVERWHELMED)
		{
			// FLEE SPRINT — key v5 fix
			unit.Expansion_GetUp(true);
			unit.SetMovementSpeedLimit(settings.FleeSpeedLimit);
			m_SoZ_DetectedSprintTimer = settings.DetectedSprintDuration;
			m_SoZ_DetectedSprintSpeed = settings.FleeSpeedLimit;
			m_SoZ_AvoidRecalcTimer = 0; // Force immediate avoidance recalc

			if (m_SoZ_InStealth)
			{
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT stealth → FLEE SPRINT at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_StealthReason = 0;
			}
			else
			{
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " FLEE SPRINT at " + unit.GetPosition());
			}

			// Avoidance: perpendicular away from nearest threat
			eAITarget fleeTarget = dangerZombie;
			if (!fleeTarget)
				fleeTarget = enemyPlayer;

			if (fleeTarget)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector myPos = unit.GetPosition();
					vector targetPos = fleeTarget.GetPosition(true);
					vector toTarget = vector.Direction(myPos, targetPos);
					toTarget[1] = 0;
					toTarget.Normalize();

					vector perpLeft = Vector(-toTarget[2], 0, toTarget[0]);
					vector perpRight = Vector(toTarget[2], 0, -toTarget[0]);
					vector moveDir = unit.GetDirection();

					vector avoidDir;
					if (vector.Dot(moveDir, perpLeft) > vector.Dot(moveDir, perpRight))
						avoidDir = perpLeft;
					else
						avoidDir = perpRight;

					avoidDir = (avoidDir * 0.7 + moveDir * 0.3);
					avoidDir.Normalize();

					m_SoZ_AvoidTarget = myPos + avoidDir * 20.0;
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
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT stealth → PLAYER CLOSE SPRINT at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_StealthReason = 0;
			}
			else
			{
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " PLAYER CLOSE SPRINT at " + unit.GetPosition());
			}

			// Avoidance: perpendicular away from player
			if (enemyPlayer)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector myPos2 = unit.GetPosition();
					vector playerPos = enemyPlayer.GetPosition(true);
					vector toPlayer = vector.Direction(myPos2, playerPos);
					toPlayer[1] = 0;
					toPlayer.Normalize();

					vector pLeft = Vector(-toPlayer[2], 0, toPlayer[0]);
					vector pRight = Vector(toPlayer[2], 0, -toPlayer[0]);
					vector mDir = unit.GetDirection();

					vector pAvoidDir;
					if (vector.Dot(mDir, pLeft) > vector.Dot(mDir, pRight))
						pAvoidDir = pLeft;
					else
						pAvoidDir = pRight;

					pAvoidDir = (pAvoidDir * 0.7 + mDir * 0.3);
					pAvoidDir.Normalize();

					m_SoZ_AvoidTarget = myPos2 + pAvoidDir * 20.0;
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
			// ZOMBIE ALERTED — jog away, don't crouch
			unit.Expansion_GetUp(true);
			unit.SetMovementSpeedLimit(settings.ZombieAlertedSpeedLimit);
			m_SoZ_AvoidRecalcTimer = 0; // Force immediate avoidance recalc

			if (m_SoZ_InStealth)
			{
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT stealth → ZOMBIE ALERTED JOG at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthCooldown = 0;
				m_SoZ_StealthReason = 0;
			}

			// Avoidance: perpendicular away from zombie
			if (dangerZombie)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector myPos3 = unit.GetPosition();
					vector zombiePos = dangerZombie.GetPosition(true);
					vector toZombie = vector.Direction(myPos3, zombiePos);
					toZombie[1] = 0;
					toZombie.Normalize();

					vector zLeft = Vector(-toZombie[2], 0, toZombie[0]);
					vector zRight = Vector(toZombie[2], 0, -toZombie[0]);
					vector zMoveDir = unit.GetDirection();

					vector zAvoidDir;
					if (vector.Dot(zMoveDir, zLeft) > vector.Dot(zMoveDir, zRight))
						zAvoidDir = zLeft;
					else
						zAvoidDir = zRight;

					zAvoidDir = (zAvoidDir * 0.7 + zMoveDir * 0.3);
					zAvoidDir.Normalize();

					m_SoZ_AvoidTarget = myPos3 + zAvoidDir * 20.0;
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
			// UNDETECTED: v4 stealth behavior — crouch + walk + avoidance
			eAITarget avoidTarget = null;
			int reason = 0;

			if (dangerZombie)
			{
				avoidTarget = dangerZombie;
				reason = 1;
			}
			else if (enemyPlayer)
			{
				avoidTarget = enemyPlayer;
				reason = 2;
			}
			else
			{
				reason = 3; // noise only
			}

			if (!m_SoZ_InStealth || reason != m_SoZ_StealthReason)
			{
				string reasonStr;
				if (reason == 1) reasonStr = "zombie nearby";
				else if (reason == 2) reasonStr = "player nearby";
				else reasonStr = "noise alert";
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " ENTER stealth (" + reasonStr + ") at " + unit.GetPosition());
				m_SoZ_InStealth = true;
				m_SoZ_StealthReason = reason;
				m_SoZ_IsAvoiding = false;
				m_SoZ_AvoidRecalcTimer = 0;
			}

			// Crouch + slow
			if (settings.EnableStealthCrouch)
				unit.OverrideStance(DayZPlayerConstants.STANCEIDX_CROUCH, false);
			unit.SetMovementSpeedLimit(settings.StealthSpeedLimit);
			if (reason != 3)
				m_SoZ_StealthCooldown = settings.StealthCooldownSeconds;

			// Avoidance (zombie or player — not noise-only)
			if (avoidTarget)
			{
				m_SoZ_AvoidRecalcTimer -= DeltaTime;
				if (m_SoZ_AvoidRecalcTimer <= 0)
				{
					vector myPos4 = unit.GetPosition();
					vector targetPos4 = avoidTarget.GetPosition(true);
					vector toTarget4 = vector.Direction(myPos4, targetPos4);
					toTarget4[1] = 0;
					toTarget4.Normalize();

					vector perpLeft4 = Vector(-toTarget4[2], 0, toTarget4[0]);
					vector perpRight4 = Vector(toTarget4[2], 0, -toTarget4[0]);
					vector moveDir4 = unit.GetDirection();

					vector avoidDir4;
					if (vector.Dot(moveDir4, perpLeft4) > vector.Dot(moveDir4, perpRight4))
						avoidDir4 = perpLeft4;
					else
						avoidDir4 = perpRight4;

					avoidDir4 = (avoidDir4 * 0.7 + moveDir4 * 0.3);
					avoidDir4.Normalize();

					m_SoZ_AvoidTarget = myPos4 + avoidDir4 * 20.0;
					m_SoZ_AvoidRecalcTimer = 3.0;

					if (!m_SoZ_IsAvoiding)
					{
						PrintToRPT("[SoZ] " + unit.GetDisplayName() + " AVOID target at " + myPos4);
						m_SoZ_IsAvoiding = true;
					}
				}
				unit.OverrideTargetPosition(m_SoZ_AvoidTarget, false);

				// Look at the threat while avoiding
				if (reason == 2)
					unit.LookAtPosition(avoidTarget.GetPosition(true), true);
			}
			else if (noiseAlert)
			{
				// Noise only — duck and look toward noise, don't change path
				unit.LookAtPosition(unit.m_SoZ_NoiseAlertDirection, true);
			}

			// Tick down noise timer
			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else if (m_SoZ_InStealth)
		{
			// Stealth cooldown — gradually exit
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
				unit.SetMovementSpeedLimit(unit.m_eAI_SpeedLimitPreference);
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " EXIT stealth (clear) at " + unit.GetPosition());
				m_SoZ_InStealth = false;
				m_SoZ_StealthReason = 0;
			}

			// Tick down noise timer in cooldown too
			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}
		else
		{
			// NO_THREAT — patrol speed variety
			int speed = unit.SoZ_GetPatrolSpeed(DeltaTime);
			bool sprinting = (speed > unit.m_eAI_SpeedLimitPreference);

			if (sprinting)
			{
				unit.eAI_SetBestSpeedLimit(speed);
			}
			else if (m_SoZ_WasPatrolSprinting)
			{
				// Sprint burst just ended — group-safe restore
				unit.SetMovementSpeedLimit(1);
				unit.eAI_SetBestSpeedLimit(unit.m_eAI_SpeedLimitPreference);
				PrintToRPT("[SoZ] " + unit.GetDisplayName() + " END patrol sprint at " + unit.GetPosition());
			}

			m_SoZ_WasPatrolSprinting = sprinting;

			// Tick down noise timer
			if (unit.m_SoZ_NoiseAlertTimer > 0)
				unit.m_SoZ_NoiseAlertTimer -= DeltaTime;
		}

		return result;
	}
};
