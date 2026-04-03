class SoZSmartAISettings
{
	static const int VERSION = 4;
	static ref SoZSmartAISettings s_Instance;

	int m_Version;

	// v1: Zombie stealth
	float ZombieAwarenessRadius;
	float ZombieAwarenessRadiusLowHealth;
	bool PreferMeleeVsZombies;
	float MeleePreferenceMaxDistance;
	float LowHealthFleeThreshold;
	int FleeZombieCountThreshold;
	float StealthCooldownSeconds;
	bool EnableStealthCrouch;
	int StealthSpeedLimit;

	// v3: Player awareness + noise + safe looting
	bool EnablePlayerStealth;
	float PlayerAwarenessRadius;
	bool EnableNoiseAlert;
	float NoiseAlertDuration;
	float SafeLootingRadius;

	// v4: Detection awareness
	bool EnableDetectionAwareness;
	float PlayerCloseDetectionRadius;
	int ZombieAlertedSpeedLimit;
	int FleeSpeedLimit;
	int DetectedByPlayerSpeedLimit;
	float DetectedSprintDuration;

	// v4: Patrol speed variety
	bool EnablePatrolSpeedVariety;
	float PatrolSprintChance;
	float PatrolSprintDurationMin;
	float PatrolSprintDurationMax;

	static SoZSmartAISettings Get()
	{
		if (!s_Instance)
			Load();

		return s_Instance;
	}

	static void Load()
	{
		s_Instance = new SoZSmartAISettings();
		s_Instance.Defaults();

		string path = "$profile:SoZ_SmartAI/SoZSmartAISettings.json";

		if (FileExist(path))
		{
			if (!ExpansionJsonFileParser<SoZSmartAISettings>.Load(path, s_Instance))
			{
				PrintToRPT("[SoZ_SmartAI] Failed to load settings, using defaults");
				s_Instance.Defaults();
				s_Instance.Save();
			}
			else
			{
				bool save = false;

				if (s_Instance.m_Version < VERSION)
				{
					PrintToRPT("[SoZ_SmartAI] Upgrading settings from v" + s_Instance.m_Version + " to v" + VERSION);
					SoZSmartAISettings defaults = new SoZSmartAISettings();
					defaults.Defaults();

					if (!s_Instance.ZombieAwarenessRadius)
						s_Instance.ZombieAwarenessRadius = defaults.ZombieAwarenessRadius;
					if (!s_Instance.ZombieAwarenessRadiusLowHealth)
						s_Instance.ZombieAwarenessRadiusLowHealth = defaults.ZombieAwarenessRadiusLowHealth;
					if (!s_Instance.MeleePreferenceMaxDistance)
						s_Instance.MeleePreferenceMaxDistance = defaults.MeleePreferenceMaxDistance;
					if (!s_Instance.LowHealthFleeThreshold)
						s_Instance.LowHealthFleeThreshold = defaults.LowHealthFleeThreshold;
					if (!s_Instance.StealthSpeedLimit)
						s_Instance.StealthSpeedLimit = defaults.StealthSpeedLimit;
					if (!s_Instance.FleeZombieCountThreshold)
						s_Instance.FleeZombieCountThreshold = defaults.FleeZombieCountThreshold;
					if (!s_Instance.StealthCooldownSeconds)
						s_Instance.StealthCooldownSeconds = defaults.StealthCooldownSeconds;

					// v3 fields
					if (!s_Instance.PlayerAwarenessRadius)
						s_Instance.PlayerAwarenessRadius = defaults.PlayerAwarenessRadius;
					if (!s_Instance.NoiseAlertDuration)
						s_Instance.NoiseAlertDuration = defaults.NoiseAlertDuration;
					if (!s_Instance.SafeLootingRadius)
						s_Instance.SafeLootingRadius = defaults.SafeLootingRadius;

					// v4 fields
					if (!s_Instance.PlayerCloseDetectionRadius)
						s_Instance.PlayerCloseDetectionRadius = defaults.PlayerCloseDetectionRadius;
					if (!s_Instance.ZombieAlertedSpeedLimit)
						s_Instance.ZombieAlertedSpeedLimit = defaults.ZombieAlertedSpeedLimit;
					if (!s_Instance.FleeSpeedLimit)
						s_Instance.FleeSpeedLimit = defaults.FleeSpeedLimit;
					if (!s_Instance.DetectedByPlayerSpeedLimit)
						s_Instance.DetectedByPlayerSpeedLimit = defaults.DetectedByPlayerSpeedLimit;
					if (!s_Instance.DetectedSprintDuration)
						s_Instance.DetectedSprintDuration = defaults.DetectedSprintDuration;
					if (!s_Instance.PatrolSprintChance)
						s_Instance.PatrolSprintChance = defaults.PatrolSprintChance;
					if (!s_Instance.PatrolSprintDurationMin)
						s_Instance.PatrolSprintDurationMin = defaults.PatrolSprintDurationMin;
					if (!s_Instance.PatrolSprintDurationMax)
						s_Instance.PatrolSprintDurationMax = defaults.PatrolSprintDurationMax;

					s_Instance.m_Version = VERSION;
					save = true;
				}

				if (save)
					s_Instance.Save();
			}
		}
		else
		{
			PrintToRPT("[SoZ_SmartAI] No settings file found, creating defaults at " + path);
			s_Instance.Save();
		}

		PrintToRPT("[SoZ_SmartAI] Settings loaded - StealthCrouch:" + s_Instance.EnableStealthCrouch + " PlayerStealth:" + s_Instance.EnablePlayerStealth + " DetectionAwareness:" + s_Instance.EnableDetectionAwareness + " PatrolVariety:" + s_Instance.EnablePatrolSpeedVariety + " FleeSpeed:" + s_Instance.FleeSpeedLimit);
	}

	void Defaults()
	{
		m_Version = VERSION;
		ZombieAwarenessRadius = 35.0;
		ZombieAwarenessRadiusLowHealth = 50.0;
		PreferMeleeVsZombies = true;
		MeleePreferenceMaxDistance = 5.0;
		LowHealthFleeThreshold = 0.3;
		FleeZombieCountThreshold = 3;
		StealthCooldownSeconds = 8.0;
		EnableStealthCrouch = true;
		StealthSpeedLimit = 1;

		EnablePlayerStealth = true;
		PlayerAwarenessRadius = 50.0;
		EnableNoiseAlert = true;
		NoiseAlertDuration = 10.0;
		SafeLootingRadius = 35.0;

		EnableDetectionAwareness = true;
		PlayerCloseDetectionRadius = 25.0;
		ZombieAlertedSpeedLimit = 2;
		FleeSpeedLimit = 3;
		DetectedByPlayerSpeedLimit = 3;
		DetectedSprintDuration = 5.0;

		EnablePatrolSpeedVariety = true;
		PatrolSprintChance = 0.08;
		PatrolSprintDurationMin = 3.0;
		PatrolSprintDurationMax = 8.0;
	}

	void Save()
	{
		string dir = "$profile:SoZ_SmartAI";
		if (!FileExist(dir))
			MakeDirectory(dir);

		JsonFileLoader<SoZSmartAISettings>.JsonSaveFile("$profile:SoZ_SmartAI/SoZSmartAISettings.json", this);
	}
};
