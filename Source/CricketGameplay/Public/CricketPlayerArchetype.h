#pragma once

#include "CoreMinimal.h"
#include "CricketPlayerArchetype.generated.h"

/**
 * ECricketPlayerArchetype — the visual/animation archetype a character presents
 * right now. This is a PRESENTATION concept, not a fixed gameplay role: the same
 * player switches archetype as the match moves them between disciplines (a pace
 * bowler is FastBowler while bowling, Batter while batting, Fielder otherwise).
 * It selects which Animation Layer (idle stance, run-up style, default loadout)
 * a UCricketCharacterAnimComponent-driven character links, never gameplay stats.
 */
UENUM(BlueprintType)
enum class ECricketPlayerArchetype : uint8
{
	Batter      UMETA(DisplayName = "Batter"),
	FastBowler  UMETA(DisplayName = "Fast Bowler"),
	Spinner     UMETA(DisplayName = "Spinner"),
	Fielder     UMETA(DisplayName = "Fielder")
};
