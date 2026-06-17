#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CricketMatchTypes.h"
#include "CricketGameMode.generated.h"

/**
 * ACricketGameMode — owns the match ruleset and (in later phases) drives the
 * ball-by-ball state machine. MVP: holds the T20 rules and the two-team setup.
 */
UCLASS()
class CRICKETSIM_API ACricketGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACricketGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cricket|Rules")
	FCricketMatchRules Rules;
};
