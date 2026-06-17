#pragma once

#include "CoreMinimal.h"
#include "CricketMatchTypes.h"
#include "CricketScoringTypes.generated.h"

/**
 * CricketScoringTypes — the data models for the match (rules) layer.
 *
 * This layer NEVER alters physics. The contract is FCricketDeliveryOutcome: the
 * already-interpreted result of one physically-simulated ball (how many runs the
 * ball physics + fielding produced, whether it was an extra, whether a wicket
 * fell). The match engine reads that and applies cricket law — scoring, strike,
 * overs, wickets, innings, result — and nothing else.
 *
 * Reuses ECricketDismissal, ECricketDeliveryLegality, FCricketMatchRules and
 * FCricketInningsState from CricketMatchTypes.h.
 */

/** Top-level match state machine. */
UENUM(BlueprintType)
enum class ECricketMatchState : uint8
{
	PreMatch      UMETA(DisplayName = "Pre-Match"),
	Toss          UMETA(DisplayName = "Toss"),
	FirstInnings  UMETA(DisplayName = "First Innings"),
	InningsBreak  UMETA(DisplayName = "Innings Break"),
	SecondInnings UMETA(DisplayName = "Second Innings"),
	MatchComplete UMETA(DisplayName = "Match Complete")
};

/** Extra runs that are NOT wides/no-balls (those are the delivery's Legality). */
UENUM(BlueprintType)
enum class ECricketExtraType : uint8
{
	None   UMETA(DisplayName = "None"),
	Bye    UMETA(DisplayName = "Bye"),
	LegBye UMETA(DisplayName = "Leg Bye")
};

/**
 * FCricketDeliveryOutcome — the interpreted result of ONE delivery, handed to the
 * match engine. Produced by interpreting the physics/fielding outcome of the ball;
 * the engine treats it as ground truth and only applies the laws.
 *
 * Run accounting (so the engine and the interpreter agree exactly):
 *   - Legality Wide/NoBall each add 1 penalty run (an extra) and are re-bowled.
 *   - RunsOffBat: runs credited to the striker (0..6). 0 on a wide. A boundary is
 *     RunsOffBat 4/6 with bBoundary set.
 *   - RanExtraRuns: runs physically run that are extras — byes/leg-byes (ExtraType)
 *     on a legal/no-ball, or the runs run on a wide.
 *   - A dismissal is applied after the runs that were completed before it.
 */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketDeliveryOutcome
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") ECricketDeliveryLegality Legality = ECricketDeliveryLegality::Legal;

	/** Runs credited to the striker (0..6). Ignored for caught/bowled/lbw/stumped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") int32 RunsOffBat = 0;

	/** True when RunsOffBat reached the rope (for fours/sixes stats; no running). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") bool bBoundary = false;

	/** Runs run that are extras (byes/leg-byes, or runs run on a wide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") int32 RanExtraRuns = 0;

	/** Classifies RanExtraRuns on a legal / no-ball delivery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") ECricketExtraType ExtraType = ECricketExtraType::None;

	/** Dismissal on this ball (NotOut for none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") ECricketDismissal Dismissal = ECricketDismissal::NotOut;

	/** For a run out, whether the STRIKER (true) or the non-striker (false) was out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery") bool bDismissedStriker = true;

	// --- Convenience builders (read like a scorebook) -----------------------
	static FCricketDeliveryOutcome Dot()              { return FCricketDeliveryOutcome(); }
	static FCricketDeliveryOutcome Runs(int32 N)      { FCricketDeliveryOutcome O; O.RunsOffBat = N; return O; }
	static FCricketDeliveryOutcome Four()             { FCricketDeliveryOutcome O; O.RunsOffBat = 4; O.bBoundary = true; return O; }
	static FCricketDeliveryOutcome Six()              { FCricketDeliveryOutcome O; O.RunsOffBat = 6; O.bBoundary = true; return O; }
	static FCricketDeliveryOutcome Wide(int32 Ran = 0){ FCricketDeliveryOutcome O; O.Legality = ECricketDeliveryLegality::Wide; O.RanExtraRuns = Ran; return O; }
	static FCricketDeliveryOutcome NoBall(int32 Bat = 0){ FCricketDeliveryOutcome O; O.Legality = ECricketDeliveryLegality::NoBall; O.RunsOffBat = Bat; return O; }
	static FCricketDeliveryOutcome Bye(int32 N)       { FCricketDeliveryOutcome O; O.RanExtraRuns = N; O.ExtraType = ECricketExtraType::Bye; return O; }
	static FCricketDeliveryOutcome LegBye(int32 N)    { FCricketDeliveryOutcome O; O.RanExtraRuns = N; O.ExtraType = ECricketExtraType::LegBye; return O; }
	static FCricketDeliveryOutcome Out(ECricketDismissal D) { FCricketDeliveryOutcome O; O.Dismissal = D; return O; }
};

/** Batting line in a scorecard. */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketBatterStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Batting") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") int32 Balls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") int32 Fours = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") int32 Sixes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") bool bOut = false;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") bool bHasBatted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") ECricketDismissal Dismissal = ECricketDismissal::NotOut;
	UPROPERTY(BlueprintReadOnly, Category = "Batting") FString BowlerName; // who got him (if applicable)

	double StrikeRate() const { return Balls > 0 ? (100.0 * Runs / Balls) : 0.0; }
};

/** Bowling line in a scorecard. */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketBowlerStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Bowling") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Bowling") int32 LegalBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bowling") int32 Maidens = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bowling") int32 RunsConceded = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bowling") int32 Wickets = 0;
	/** Runs conceded in the current over (for the maiden check). Reset each over. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling") int32 RunsThisOver = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bowling") int32 BallsThisOver = 0;

	int32 CompletedOvers() const { return LegalBalls / 6; }
	int32 BallsInPartOver() const { return LegalBalls % 6; }
	double OversDecimal() const { return CompletedOvers() + BallsInPartOver() / 10.0; }
	double Economy() const { return LegalBalls > 0 ? (6.0 * RunsConceded / LegalBalls) : 0.0; }
};

/** One innings' full scorecard + live batting/bowling positions. */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketInningsScorecard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Innings") FString BattingTeam;
	UPROPERTY(BlueprintReadOnly, Category = "Innings") FString BowlingTeam;

	UPROPERTY(BlueprintReadOnly, Category = "Innings") FCricketInningsState Totals;

	UPROPERTY(BlueprintReadOnly, Category = "Innings") TArray<FCricketBatterStats> Batters;  // full order
	UPROPERTY(BlueprintReadOnly, Category = "Innings") TArray<FCricketBowlerStats> Bowlers;   // those who bowled

	// Live positions (indices into Batters / Bowlers).
	UPROPERTY(BlueprintReadOnly, Category = "Innings") int32 StrikerIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Innings") int32 NonStrikerIndex = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Innings") int32 NextBatterIndex = 2;
	UPROPERTY(BlueprintReadOnly, Category = "Innings") int32 CurrentBowler = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Innings") bool bAllOut = false;
	UPROPERTY(BlueprintReadOnly, Category = "Innings") bool bClosed = false;

	int32 CompletedOvers() const { return Totals.CompletedOvers(); }
	int32 BallsThisOver() const { return Totals.BallsThisOver(); }
	double RunRate() const { return Totals.LegalBalls > 0 ? (6.0 * Totals.Runs / Totals.LegalBalls) : 0.0; }
};

/** A team's playing XI for a match (batting order = array order). */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketSquad
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad") FString TeamName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad") FString ShortCode;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad") TArray<FString> PlayerNames;
};

/** Final (or in-progress) result of the match. */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketMatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result") bool bDecided = false;
	UPROPERTY(BlueprintReadOnly, Category = "Result") bool bTie = false;
	UPROPERTY(BlueprintReadOnly, Category = "Result") FString WinningTeam;
	/** Human-readable, e.g. "India won by 5 wickets" / "Match tied". */
	UPROPERTY(BlueprintReadOnly, Category = "Result") FString Summary;
};
