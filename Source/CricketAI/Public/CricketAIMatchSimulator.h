#pragma once

#include "CoreMinimal.h"
#include "CricketAIPlayerProfile.h"
#include "CricketScoringTypes.h"     // FCricketSquad
#include "CricketMatchTypes.h"       // FCricketMatchRules, ECricketDismissal
#include "CricketBatterBrain.h"      // ECricketBatterAction
#include "CricketBowlingTypes.h"     // ECricketLength
#include "CricketBalanceConfig.h"    // FCricketBalanceConfig
#include "CricketAIMatchSimulator.generated.h"

class UCricketMatchEngine;
class UCricketTeamDataAsset;

/**
 * FCricketAITeam — a side as the AI sees it: the engine squad (names + order), one
 * AI personality per player, and the team strategy. Built from the existing roster
 * asset so the shipped India/Australia teams play with full AI for free.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketAITeam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AI|Sim") FCricketSquad Squad;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Sim") TArray<FCricketAIPlayerProfile> Profiles;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Sim") FCricketTeamStrategy Strategy;

	const FCricketAIPlayerProfile& ProfileByName(const FString& Name) const;

	/** Derive a full AI team from a roster data asset (profiles + a default strategy). */
	static FCricketAITeam FromTeamData(const UCricketTeamDataAsset& Data, ECricketAIDifficulty Difficulty);

	/** Build directly from a squad + per-player profiles (used by tests). */
	static FCricketAITeam FromProfiles(const FCricketSquad& InSquad, const TArray<FCricketAIPlayerProfile>& InProfiles, const FCricketTeamStrategy& InStrategy);
};

/**
 * FCricketAIInningsTelemetry — the validation record for one simulated innings:
 * the score, the spread of batter intents, the bowler's length pattern, the
 * boundaries and the dismissal modes. The automated tests assert these fall in
 * believable cricket ranges.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketAIInningsTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") FString BattingTeam;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 Wickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 LegalBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 Fours = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 Sixes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 Dots = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 Extras = 0;

	/** Counts indexed by ECricketBatterAction (Leave..Boundary). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") TArray<int32> ActionCounts;
	/** Counts indexed by ECricketLength (FullToss..Bouncer). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") TArray<int32> BowlLengthCounts;
	/** Counts indexed by ECricketDismissal (NotOut..Stumped). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") TArray<int32> Dismissals;

	// --- Phase splits (for powerplay / death AI validation) ---
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 PowerplayRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 PowerplayWickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 PowerplayBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 MiddleRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 MiddleWickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 MiddleBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 DeathRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 DeathWickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 DeathBalls = 0;

	double RunRate() const { return LegalBalls > 0 ? (6.0 * Runs / LegalBalls) : 0.0; }
	double PowerplayRunRate() const { return PowerplayBalls > 0 ? (6.0 * PowerplayRuns / PowerplayBalls) : 0.0; }
	double DeathRunRate() const { return DeathBalls > 0 ? (6.0 * DeathRuns / DeathBalls) : 0.0; }
	/** Boundary balls as a fraction of legal balls (fours + sixes). */
	double BoundaryFraction() const { return LegalBalls > 0 ? (double)(Fours + Sixes) / LegalBalls : 0.0; }
	double DotFraction() const { return LegalBalls > 0 ? (double)Dots / LegalBalls : 0.0; }
};

/** The full record of a simulated match. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketAIMatchTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") TArray<FCricketAIInningsTelemetry> Innings;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") FString ResultSummary;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") int32 TotalBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") bool bCompleted = false;

	/** True if the side batting SECOND won (a successful chase). False on a defended total or tie. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") bool bChaseSucceeded = false;
	/** True if the match was tied. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Sim") bool bTie = false;
};

/**
 * FCricketAIMatchSimulator — the automated match simulation and the believability
 * engine. It plays a complete T20 deterministically by wiring the AI brains to the
 * REAL Match Engine: the captain rotates bowlers, the bowler brain chooses each
 * delivery, the batter brain chooses each response, the contest model resolves the
 * ball, the existing interpreter classifies it, and the engine applies the laws.
 *
 * This is exactly the path ACricketMatchRunner needs to replace its placeholder
 * generator with — same interpreter, same engine, but now fed by cricket
 * intelligence. Deterministic given the seed, so its statistical output (run rates,
 * wicket rates, bowling patterns, shot distributions) is unit-testable.
 */
class CRICKETAI_API FCricketAIMatchSimulator
{
public:
	/**
	 * Play a full match between two AI teams; returns the telemetry. The optional
	 * balance config re-tunes the contest (swing/spin/bounce/timing/aggression/risk);
	 * a neutral config reproduces the shipped behaviour exactly.
	 */
	static FCricketAIMatchTelemetry Simulate(
		const FCricketAITeam& TeamA,
		const FCricketAITeam& TeamB,
		const FCricketMatchRules& Rules,
		int32 Seed,
		bool bTeamABatsFirst = true,
		const FCricketBalanceConfig& Balance = FCricketBalanceConfig());
};
