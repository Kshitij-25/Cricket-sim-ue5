#pragma once

#include "CoreMinimal.h"
#include "CricketScoringTypes.h"   // ECricketMatchState
#include "CricketMatchSnapshot.generated.h"

class UCricketMatchEngine;

/**
 * FCricketMatchSnapshot — an immutable, read-only copy of the Match Engine's
 * scoreboard at one instant. The presentation layer's classifier diffs two snapshots
 * (before/after a ball) to discover moments; it is the single seam through which the
 * presentation reads the rules layer, and it is a COPY, so nothing presentation does
 * can touch a live outcome.
 *
 * Capture() reads a live engine (defined in the .cpp, which alone includes the engine
 * header). Tests build snapshots field-by-field, with no engine, world or RHI — the
 * same headless contract as the UI/audio view-models.
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketMatchSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketMatchState State = ECricketMatchState::PreMatch;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString BattingTeam;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString BowlingTeam;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 TeamRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 TeamWickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 LegalBalls = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString StrikerName;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 StrikerRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 StrikerBalls = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString BowlerName;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 BowlerWickets = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bChasing = false;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 Target = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 RunsRequired = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 BallsRemaining = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") double RequiredRunRate = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bResultDecided = false;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bTie = false;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString WinningTeam;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString ResultSummary;

	int32 CompletedOvers() const { return LegalBalls / 6; }
	int32 BallsThisOver() const { return LegalBalls % 6; }
	double RunRate() const { return LegalBalls > 0 ? (6.0 * TeamRuns / LegalBalls) : 0.0; }

	/** Pre-formatted overs string, e.g. "12.3". */
	FString OversText() const { return FString::Printf(TEXT("%d.%d"), CompletedOvers(), BallsThisOver()); }

	/** Read a live engine into a fresh snapshot (pure read). */
	static FCricketMatchSnapshot Capture(const UCricketMatchEngine& Engine);
};
