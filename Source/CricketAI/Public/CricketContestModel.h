#pragma once

#include "CoreMinimal.h"
#include "CricketBatterBrain.h"
#include "CricketBowlerBrain.h"
#include "CricketAIPlayerProfile.h"
#include "CricketBalanceConfig.h"        // FCricketBalanceConfig
#include "CricketOutcomeInterpreter.h"   // FCricketBallResult

/**
 * FCricketContestModel — the headless bat-vs-ball CONTEST resolver.
 *
 * The live game resolves every ball through the full physics rollout (bowling
 * release -> ball flight -> the moving bat detecting contact -> fielding). That
 * rollout needs a ticking world, so for the deterministic, headless match
 * simulator (and its statistical tests) this model stands in for it: given the two
 * brains' DECISIONS and the players' abilities it produces the same kind of raw
 * physical facts (FCricketBallResult) the physics would, which then flow through
 * the EXISTING FCricketOutcomeInterpreter and Match Engine unchanged.
 *
 * Crucially it is driven by the very same decisions the live controllers feed the
 * physics — a tempted batter who chooses the wrong shot against his weakness, a
 * bowler who nails a yorker — so the emergent run rates, wicket rates and shot
 * distributions reflect the AI's cricket, not a flat dice. It models the same
 * FACTORS the physics would weigh (length suitability, the matchup, timing risk,
 * the field) rather than scripting a chosen result.
 */
class CRICKETAI_API FCricketContestModel
{
public:
	/**
	 * Resolve one delivery into raw physical facts for the interpreter. The optional
	 * balance config scales the physical factors (movement threat, timing scatter,
	 * boundary/wicket odds); a default-constructed (neutral) config reproduces the
	 * shipped behaviour exactly.
	 */
	static FCricketBallResult Resolve(
		const FCricketBowlerDecision& Bowl,
		const FCricketBatterDecision& Bat,
		const FCricketAIPlayerProfile& Bowler,
		const FCricketAIPlayerProfile& Batter,
		const FCricketDeliveryRead& Ball,
		FRandomStream& Rng,
		const FCricketBalanceConfig& Balance = FCricketBalanceConfig());
};
