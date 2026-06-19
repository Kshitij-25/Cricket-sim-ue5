#pragma once

#include "CoreMinimal.h"
#include "CricketPresentationTypes.h"

/**
 * FCricketReplayDirector — the pure "should we replay this, and how?" core.
 *
 * Given a classified moment it returns an FCricketReplayPlan: whether to roll an
 * automatic replay, the slow-motion rate, the ordered camera angles to cut through,
 * and how long to hold each. The Presentation Manager hands the plan to the existing
 * UCricketReplayComponent, which plays back the RECORDED frames — the replay never
 * re-simulates and never changes what happened.
 *
 * The rules are intentionally simple and data-like so they are easy to assert and to
 * retune: wickets and sixes always replay (multi-angle, slow); fours replay only when
 * they matter (pressure boundaries); the match-deciding moment gets the fullest,
 * slowest, multi-angle treatment.
 */
struct CRICKETPRESENTATION_API FCricketReplayDirector
{
	/** Minimum severity a boundary (four) must reach to earn an automatic replay. */
	ECricketPresentationSeverity MinBoundaryReplaySeverity = ECricketPresentationSeverity::High;

	/** Build the replay plan for a moment (an empty plan if it shouldn't replay). */
	FCricketReplayPlan BuildPlan(const FCricketPresentationEvent& Event) const;

	/** Quick predicate the manager uses to gate auto-replays. */
	bool ShouldAutoReplay(const FCricketPresentationEvent& Event) const { return BuildPlan(Event).bShouldReplay; }
};
