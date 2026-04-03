# SoZ_SmartAI

Server-side AI behavior mod for [DayZ Expansion](https://steamcommunity.com/workshop/filedetails/?id=2572331007). Makes Expansion AI act more like real survivors.

## Features (v5)

**Detection-Aware Behavior** — AI responds differently based on whether it has been detected:

| Situation | AI Response |
|---|---|
| Flee (3+ zombies / low HP) | **Sprint** away |
| Enemy player close (<25m) | **Sprint** away |
| Zombie investigating nearby (<17m) | **Jog** away, stand up |
| Threats nearby but unaware | Crouch + walk (stealth) |
| No threats | Patrol with occasional sprint bursts |

**Key Improvements:**
- Flee actually sprints (was walking with pathfinding avoidance only)
- Patrol speed variety — AI occasionally sprints during patrol
- Sticky sprint — maintains sprint for configurable duration after detection
- Group-safe speed management — respects Expansion's group braking for injured/lagging members
- Melee preference vs zombies — avoids attracting more zombies with gunfire
- Safe looting — won't loot corpses when threats are nearby
- Noise awareness — ducks on enemy gunfire/explosions

## Requirements

- DayZ 1.28+
- DayZ Expansion (AI module)
- CF + DabsFramework

## Installation

1. Copy the `SoZ_SmartAI` folder to your server's root directory as `@SoZ_SmartAI`
2. Pack the scripts into `@SoZ_SmartAI/addons/soz_smartai_scripts.pbo`
3. Add `-servermod=@SoZ_SmartAI` to your server startup parameters
4. Restart the server

This is a **server-side only** mod — no client download required.

## Configuration

Settings are auto-generated at `$profile/SoZ_SmartAI/SoZSmartAISettings.json` on first run. Settings auto-upgrade between versions.

### Detection Settings
| Setting | Default | Description |
|---|---|---|
| EnableDetectionAwareness | true | Master toggle for v5 detection logic |
| PlayerCloseDetectionRadius | 25.0 | Distance below which player is assumed to see AI |
| ZombieAlertedSpeedLimit | 2 (JOG) | Speed when alerted zombie is nearby |
| FleeSpeedLimit | 3 (SPRINT) | Speed when fleeing |
| DetectedByPlayerSpeedLimit | 3 (SPRINT) | Speed when spotted by player |
| DetectedSprintDuration | 5.0 | Seconds to maintain sprint after detection |

### Patrol Speed Variety
| Setting | Default | Description |
|---|---|---|
| EnablePatrolSpeedVariety | true | Occasional sprint bursts on patrol |
| PatrolSprintChance | 0.08 | Chance per 3s cycle (~every 37s) |
| PatrolSprintDurationMin | 3.0 | Minimum sprint burst duration |
| PatrolSprintDurationMax | 8.0 | Maximum sprint burst duration |

### Stealth Settings
| Setting | Default | Description |
|---|---|---|
| EnableStealthCrouch | true | AI crouches when threats nearby but undetected |
| StealthSpeedLimit | 1 (WALK) | Speed during stealth |
| StealthCooldownSeconds | 8.0 | Cooldown before exiting stealth |
| ZombieAwarenessRadius | 35.0 | Zombie detection radius |
| ZombieAwarenessRadiusLowHealth | 50.0 | Extended radius when health <30% |
| EnablePlayerStealth | true | Stealth behavior near enemy players |
| PlayerAwarenessRadius | 50.0 | Player detection radius |

### Combat Settings
| Setting | Default | Description |
|---|---|---|
| PreferMeleeVsZombies | true | Suppress gunfire vs zombies within melee range |
| MeleePreferenceMaxDistance | 5.0 | Max distance for melee preference |
| FleeZombieCountThreshold | 3 | Number of zombies that triggers flee |
| LowHealthFleeThreshold | 0.3 | Health % below which AI flees |
| SafeLootingRadius | 35.0 | Don't loot if threats within this radius |

## Logging

All mod activity is logged to the server RPT with the `[SoZ]` prefix:
- `[SoZ] AI_Name ENTER stealth (zombie nearby)`
- `[SoZ] AI_Name FLEE SPRINT`
- `[SoZ] AI_Name PLAYER CLOSE SPRINT`
- `[SoZ] AI_Name ZOMBIE ALERTED - JOG`
- `[SoZ] AI_Name RESTORE speed (was override)`

## How It Works

The mod operates in the **pre-combat zone** (threat < 0.4). When threat reaches 0.4+, Expansion's Fighting FSM takes over. All patrol configs should have `UnderThreatSpeed: SPRINT` for correct behavior during combat.

Speed management uses transition-based restores instead of per-tick resets to preserve Expansion's group braking logic for squads with injured or lagging members.

## License

MIT
