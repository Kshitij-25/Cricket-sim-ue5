#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketAITypes.h"
#include "CricketTeamDataAsset.h"   // FCricketPlayer, ECricketRole
#include "CricketBowlingTypes.h"    // ECricketBowlingStyle
#include "CricketAIPlayerProfile.generated.h"

/**
 * CricketAIPlayerProfile — the DATA MODELS that give each AI player a cricketing
 * personality: how a batter scores and where they are vulnerable, what a bowler
 * does with the ball, and a captain's tactical leanings. The brains read these to
 * make believable, individual decisions (an opener grinds; a finisher swings; an
 * off-spinner attacks the left-hander's pads).
 *
 * These tendencies bias the brain's CHOICE only. They never touch the physics or
 * the laws — a "weakness against the short ball" makes the batter more likely to
 * choose a risky pull, it does not script an edge. The contact, the edge and the
 * dismissal still come from the shared physics/contest model.
 *
 * Every block derives sensibly from the existing FCricketPlayer rating, so the
 * shipped India/Australia rosters get rich AI behaviour with no extra authoring;
 * authored profiles (UCricketAIProfileDataAsset) override the derivation.
 */

// ---------------------------------------------------------------------------
// Batting tendencies
// ---------------------------------------------------------------------------

/** How a batter scores, rotates and where their technique breaks down. All [0,1]. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBattingTendencies
{
	GENERATED_BODY()

	/** Baseline intent to score rather than block (an opener ~0.4, a finisher ~0.85). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Aggression = 0.5;

	/** Defensive solidity: how reliably the batter keeps out good balls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Technique = 0.5;

	/** Clean ball-striking / power for clearing the rope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Power = 0.5;

	/** Ability to manufacture singles and keep the scoreboard ticking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double StrikeRotation = 0.5;

	/** Competence against genuine pace (1 = comfortable, 0 = hops about). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double VsPace = 0.5;

	/** Competence against spin (footwork, reading turn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double VsSpin = 0.5;

	/** Susceptibility to the short ball (1 = gets in a tangle / top-edges the pull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ShortBallWeakness = 0.4;

	/** Tendency to chase the wide ball outside off (1 = nicks off regularly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double OutsideOffTemptation = 0.4;

	/** Vulnerability to the full straight ball (driving away from the body / lbw, bowled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Batting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double FullBallWeakness = 0.35;
};

// ---------------------------------------------------------------------------
// Bowling tendencies
// ---------------------------------------------------------------------------

/** What a bowler is and how well they do it. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBowlingTendencies
{
	GENERATED_BODY()

	/** The bowler's family — picks pace vs spin defaults and the available variations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling")
	ECricketBowlingStyle Style = ECricketBowlingStyle::Pace;

	/** Release pace as a fraction of an express quick (1 = ~150 km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Pace = 0.7;

	/** Control: how reliably the bowler hits the chosen line/length (lowers extras). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Control = 0.6;

	/** Lateral movement skill (swing for pace, turn for spin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Movement = 0.5;

	/** Breadth and quality of variations (slower balls, bouncers, the wrong'un). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Variation = 0.5;

	/** Death-overs nerve: yorker execution and holding lengths under pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DeathSkill = 0.5;

	/** Attacking vs containing bias (1 = always hunts the wicket). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Aggression = 0.5;

	bool IsSpin() const { return Style == ECricketBowlingStyle::OffSpin || Style == ECricketBowlingStyle::LegSpin; }
};

// ---------------------------------------------------------------------------
// Tactical preferences (captaincy) & team strategy
// ---------------------------------------------------------------------------

/** A captain's leanings — biases bowling changes and field settings. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketTacticalPreferences
{
	GENERATED_BODY()

	/** Default field aggression (1 = attacking catchers up; 0 = sweeper-heavy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double FieldAggression = 0.5;

	/** Willingness to bowl the strike bowlers early / in the powerplay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double FrontloadStrikeBowlers = 0.5;

	/** Tendency to react to a new batter with an attacking field/plan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double AttackNewBatter = 0.6;

	/** How readily the captain rotates bowlers to break a partnership. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tactics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double BowlingChangeFreq = 0.5;
};

/**
 * FCricketTeamStrategy — the team-level plan: phase-by-phase batting approach plus
 * the captain's tactical preferences and the AI difficulty the side plays at.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketTeamStrategy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
	ECricketAIDifficulty Difficulty = ECricketAIDifficulty::Hard;

	/**
	 * Intended posture in the powerplay (first 6). Consolidate, not all-out: with the
	 * field up the powerplay already scores freely, and going harder than this just
	 * threw early wickets and left the tail to bat the death — which inverted the innings
	 * shape (powerplay faster than the death). Tuned from the validation batch's phase split.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
	ECricketInningsApproach PowerplayApproach = ECricketInningsApproach::Consolidate;

	/** Intended posture through the middle overs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
	ECricketInningsApproach MiddleApproach = ECricketInningsApproach::Consolidate;

	/** Intended posture at the death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
	ECricketInningsApproach DeathApproach = ECricketInningsApproach::AllOut;

	/** A baseline target par score the side bats around when setting (first innings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy", meta = (ClampMin = "60"))
	int32 ParTotalT20 = 165;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Strategy")
	FCricketTacticalPreferences Captaincy;
};

/**
 * FCricketAIPlayerProfile — the complete AI personality of one player: identity,
 * coarse ratings, and the batting/bowling tendencies the brains reason over.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketAIPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") ECricketRole Role = ECricketRole::BatterMiddle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile", meta = (ClampMin = "0.0", ClampMax = "1.0")) double BattingRating = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile", meta = (ClampMin = "0.0", ClampMax = "1.0")) double BowlingRating = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile", meta = (ClampMin = "0.0", ClampMax = "1.0")) double FieldingRating = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") FCricketBattingTendencies Batting;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") FCricketBowlingTendencies Bowling;

	bool CanBowl() const
	{
		return BowlingRating > 0.25 || Role == ECricketRole::PaceBowler || Role == ECricketRole::SpinBowler || Role == ECricketRole::AllRounder;
	}

	/**
	 * Derive a full personality from a coarse FCricketPlayer rating record. Role and
	 * pace seed the style and tendencies so the shipped rosters behave sensibly with
	 * no extra data. Deterministic — same player in, same profile out.
	 */
	static FCricketAIPlayerProfile Derive(const FCricketPlayer& P);
};

/**
 * UCricketAIProfileDataAsset — authored AI personalities for one team, plus its
 * strategy. Optional: where present it overrides the per-player derivation; where
 * absent the brains derive from the team roster asset. One per team under
 * /Game/Data/AI.
 */
UCLASS(BlueprintType)
class CRICKETAI_API UCricketAIProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") FString TeamName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") FCricketTeamStrategy Strategy;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Profile") TArray<FCricketAIPlayerProfile> Players;

	/** Find an authored profile by player name; nullptr if not present. */
	const FCricketAIPlayerProfile* Find(const FString& PlayerName) const
	{
		for (const FCricketAIPlayerProfile& Prof : Players)
		{
			if (Prof.Name == PlayerName) { return &Prof; }
		}
		return nullptr;
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("AIProfileData"), GetFName());
	}
};
