#pragma once

#include "CoreMinimal.h"
#include "CricketPresentationTypes.h"
#include "CricketMatchSnapshot.h"
#include "CricketScorePresentationModel.generated.h"

/**
 * FCricketScorePresentationModel — the broadcast SCORE GRAPHICS brain.
 *
 * It turns the running scoreboard into the pre-formatted lines a premium broadcast
 * shows: an over summary ("Over 12: 8 runs, 1 wkt — 96/3"), the live partnership
 * ("Partnership 45 (32)"), the chase line with required run rate ("Need 48 from 30 •
 * RRR 9.60"), and milestone notifications. It is fed a read-only snapshot per ball and
 * accumulates the per-over / partnership tallies the engine doesn't keep itself.
 *
 * Pure and headless-testable: feed it before/after snapshots and assert the strings.
 * It owns no gameplay state and never writes back.
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketScorePresentationModel
{
	GENERATED_BODY()

	// --- Live partnership (since the last wicket) ----------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Score") int32 PartnershipRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Score") int32 PartnershipBalls = 0;

	// --- Current over accumulator --------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Score") int32 OverRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Score") int32 OverWickets = 0;

	// --- Latest finished-over summary (set when an over completes) -----------
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Score") FString LastOverSummary;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Score") bool bHasOverSummary = false;

	/** Clear all accumulators (call at the start of each innings). */
	void ResetInnings()
	{
		PartnershipRuns = PartnershipBalls = OverRuns = OverWickets = 0;
		LastOverSummary.Reset();
		bHasOverSummary = false;
	}

	/**
	 * Fold one delivery into the running graphics state. Returns true if this ball
	 * completed an over (so LastOverSummary was refreshed).
	 */
	bool OnBall(const FCricketMatchSnapshot& Before, const FCricketMatchSnapshot& After);

	/** "Partnership 45 (32)" — current unbroken stand. */
	FString PartnershipText() const;

	/** "Need 48 from 30 • RRR 9.60" (empty when not chasing). */
	static FString ChaseText(const FCricketMatchSnapshot& Snapshot);

	/** A milestone notification line, e.g. "FIFTY • Kohli (34 balls)" — from an event. */
	static FString MilestoneText(const FCricketPresentationEvent& Event, const FCricketMatchSnapshot& After);
};
