#pragma once

#include "CoreMinimal.h"

struct FCricketBallState;
struct FCricketSurfacePatch;
struct FCricketImpact;
struct FCricketBounceContext;
struct FCricketBounceReport;

/**
 * FCricketSeamSolver — SEAM movement off the pitch.
 *
 * When the ball lands on its seam, the proud seam grips the surface and kicks
 * the ball sideways along the seam's tangential projection. The magnitude scales
 * with how flush the seam strikes (Impact.SeamContact), incoming pace, and a
 * surface seam-grip term that is strongest on GREEN (grassy), firm, slightly
 * abrasive pitches — which is exactly where seam bowlers thrive.
 *
 * Wobble seam: a scrambled seam (low FCricketBallState::SeamStability) lands at
 * an unpredictable angle, so both the DIRECTION and the MAGNITUDE of the kick
 * vary with the (deterministic) Variance input — late, inconsistent movement
 * either way off the straight, rather than the repeatable one-way deviation of a
 * held seam. This is the "wobble seam" behaviour, emergent from seam stability.
 *
 * Writes Context.SeamImpulse and the report's SeamDeviationMS. Does not touch
 * spin or the normal response.
 */
class CRICKETPHYSICS_API FCricketSeamSolver
{
public:
	static void Solve(
		const FCricketBallState& State,
		const FCricketSurfacePatch& Patch,
		const FCricketImpact& Impact,
		FCricketBounceContext& Context,
		FCricketBounceReport& Report);
};
