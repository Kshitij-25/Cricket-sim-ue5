#pragma once

#include "CoreMinimal.h"
#include "CricketPresentationTypes.h"
#include "CricketMatchSnapshot.h"

struct FCricketDeliveryOutcome;

/**
 * FCricketEventClassifier — the pure "what just happened, and is it worth showing?"
 * core of the presentation layer.
 *
 * It takes the interpreted ball (FCricketDeliveryOutcome) and the scoreboard BEFORE
 * and AFTER that ball, and emits zero or more FCricketPresentationEvents: a four, a
 * six, a wicket, a crossed milestone (fifty/century/three-for/five-for/team hundred),
 * and the match-deciding moment. It is the analogue of FCricketAudioSelector for the
 * broadcast layer — entirely static and stateless, so it is asserted headlessly with
 * hand-built snapshots and never touches the engine.
 *
 * It is READ-ONLY by construction: a snapshot is a copy, and the classifier returns
 * new structs. Nothing here can alter a scoreboard.
 */
struct CRICKETPRESENTATION_API FCricketEventClassifier
{
	/**
	 * Classify one delivery. `Before`/`After` are scoreboards either side of the ball
	 * (After == the engine's state once OnBallApplied fired). Returns the moments in
	 * priority order (a wicket before a milestone before a boundary), so the first
	 * element is the headline beat for the ball.
	 */
	static TArray<FCricketPresentationEvent> ClassifyDelivery(
		const FCricketDeliveryOutcome& Outcome,
		const FCricketMatchSnapshot& Before,
		const FCricketMatchSnapshot& After);

	/** The single match-result moment when a match has just been decided. */
	static FCricketPresentationEvent ClassifyResult(const FCricketMatchSnapshot& After);

	// --- Building blocks (exposed for focused tests) -------------------------

	/** Boundary (four) or six from the outcome; None type if neither. */
	static FCricketPresentationEvent ClassifyBoundary(
		const FCricketDeliveryOutcome& Outcome, const FCricketMatchSnapshot& After);

	/** A wicket event from the outcome; None type if no dismissal. */
	static FCricketPresentationEvent ClassifyWicket(
		const FCricketDeliveryOutcome& Outcome, const FCricketMatchSnapshot& Before, const FCricketMatchSnapshot& After);

	/** The highest milestone crossed on this ball, or None. */
	static FCricketPresentationEvent ClassifyMilestone(
		const FCricketMatchSnapshot& Before, const FCricketMatchSnapshot& After);

	/** Map a (chase-aware) severity to the crowd impulse it injects. */
	static float CrowdImpulseFor(ECricketPresentationEventType Type, ECricketPresentationSeverity Severity);
};
