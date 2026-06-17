#pragma once

#include "CoreMinimal.h"
#include "CricketMatchTypes.generated.h"

/** How a batter was dismissed (MVP set; emerges from ball + fielding events). */
UENUM(BlueprintType)
enum class ECricketDismissal : uint8
{
	NotOut      UMETA(DisplayName = "Not Out"),
	Bowled      UMETA(DisplayName = "Bowled"),
	Caught      UMETA(DisplayName = "Caught"),
	LBW         UMETA(DisplayName = "LBW"),
	RunOut      UMETA(DisplayName = "Run Out"),
	Stumped     UMETA(DisplayName = "Stumped")
};

/** Legality of a delivery — drives extras and re-bowls. */
UENUM(BlueprintType)
enum class ECricketDeliveryLegality : uint8
{
	Legal    UMETA(DisplayName = "Legal"),
	Wide     UMETA(DisplayName = "Wide"),
	NoBall   UMETA(DisplayName = "No Ball")
};

/** Coarse phase of the innings, used by AI and (later) presentation. */
UENUM(BlueprintType)
enum class ECricketMatchPhase : uint8
{
	PreMatch     UMETA(DisplayName = "Pre-match"),
	Powerplay    UMETA(DisplayName = "Powerplay"),
	Middle       UMETA(DisplayName = "Middle Overs"),
	Death        UMETA(DisplayName = "Death Overs"),
	InningsBreak UMETA(DisplayName = "Innings Break"),
	Complete     UMETA(DisplayName = "Complete")
};

/**
 * FCricketMatchRules — T20 ruleset. Centralised so the format is data, not
 * scattered magic numbers. Defaults are standard men's T20.
 */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketMatchRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 OversPerInnings = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 BallsPerOver = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 PlayersPerTeam = 11;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 PowerplayOvers = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 MaxOversPerBowler = 4;
	/** Fielders allowed outside the 30-yard circle during the powerplay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 PowerplayFieldersOut = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") int32 NonPowerplayFieldersOut = 5;
};

/** Live innings tally. Kept as plain data, mutated only by the rules engine. */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketInningsState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Wickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 LegalBalls = 0; // balls bowled this innings
	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Extras = 0;

	int32 CompletedOvers() const { return LegalBalls / 6; }
	int32 BallsThisOver() const { return LegalBalls % 6; }
};
