#pragma once

#include "CoreMinimal.h"
#include "CricketPresentationTypes.h"
#include "CricketMatchSnapshot.h"

/**
 * FCricketMatchFlowModel — the MATCH FLOW PRESENTATION LAYER.
 *
 * It assembles the broadcast-package sequences that bracket the cricket: the match
 * intro, the team intros, the toss, over and innings transitions, and the result
 * presentation. Each is a pure list of captioned, timed camera steps
 * (FCricketBroadcastSequence) the Presentation Manager plays back — purely cosmetic
 * overlays that never gate or alter a ball.
 *
 * Stateless and deterministic: hand it team names / a snapshot and it returns the
 * sequence, so the whole broadcast script is unit-tested headlessly.
 */
struct CRICKETPRESENTATION_API FCricketMatchFlowModel
{
	/** "INDIA vs AUSTRALIA — T20" opener over the master shot. */
	static FCricketBroadcastSequence BuildMatchIntro(const FString& TeamA, const FString& TeamB, int32 OversPerInnings);

	/** A team's line-up flashed over the batting/bowling angle. */
	static FCricketBroadcastSequence BuildTeamIntro(const FString& TeamName, const TArray<FString>& PlayerNames);

	/** "AUSTRALIA won the toss and chose to bowl." */
	static FCricketBroadcastSequence BuildToss(const FString& TossWinner, bool bChoseToBat);

	/** Short between-overs bumper with the over summary + chase line. */
	static FCricketBroadcastSequence BuildOverTransition(const FString& OverSummary, const FString& ChaseLine);

	/** Innings break: the first-innings total and the target to chase. */
	static FCricketBroadcastSequence BuildInningsTransition(const FString& BattingTeam, int32 Runs, int32 Wickets, int32 Target);

	/** The closing result presentation, the slowest and longest of the beats. */
	static FCricketBroadcastSequence BuildMatchResult(const FCricketMatchSnapshot& Final);
};
