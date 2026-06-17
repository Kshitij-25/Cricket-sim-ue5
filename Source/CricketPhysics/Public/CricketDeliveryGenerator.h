#pragma once

#include "CoreMinimal.h"
#include "CricketBowlingTypes.h"

/**
 * FCricketDeliveryGenerator
 *
 * Turns a bowling INTENT (line, length, pace, movement, swing/spin amount) plus
 * a bowler's ACTION and the world CONTEXT into a complete set of physical
 * FCricketReleaseParameters. It is the bowling analogue of FCricketShotGenerator:
 * it never decides an outcome, it only sets the ball's condition at release.
 *
 * The two non-trivial mappings are:
 *   - LENGTH -> release elevation: solved by bisection against the real
 *     FCricketTrajectoryPredictor so the ball actually pitches in the chosen
 *     band (the same model the live ball flies through — never a scripted curve).
 *   - MOVEMENT -> seam/spin/surface: seam orientation, spin axis & rate, seam
 *     stability and the swing regime are authored so the desired swing/turn
 *     EMERGES from FCricketAerodynamics + FCricketPitchInteraction.
 *
 * Pure and deterministic: no UObject, no UWorld. Given the same inputs (and seed)
 * it produces identical parameters, so it is unit-testable headlessly and is safe
 * to run inside the deterministic core. Human inaccuracy is applied to the INPUTS
 * only (seeded scatter), never to the result.
 *
 * Units & axes: SI throughout; world axes +X toward the striker, +Y off side
 * (right-hander), +Z up.
 */
class CRICKETPHYSICS_API FCricketDeliveryGenerator
{
public:
	/**
	 * Generate the full release parameters for a delivery. If OutDiag is non-null
	 * it is filled with the predicted pitch point, length, line, free-flight swing
	 * and regime (all from the same integrated model).
	 */
	static FCricketReleaseParameters Generate(
		const FCricketBowlingIntent& Intent,
		const FCricketBowlingAction& Action,
		const FCricketDeliveryContext& Context,
		FCricketDeliveryDiagnostics* OutDiag = nullptr);

	// --- Pure authoring helpers (exposed for tooling & tests) ----------------

	/** Coarse family implied by a movement archetype. */
	static ECricketBowlingStyle StyleForMovement(ECricketMovement Movement);

	/** Target length (m from the striker's stumps) for a length band + family. */
	static double TargetLengthM(ECricketLength Length, ECricketBowlingStyle Style);

	/** Target aim line (signed m at the stumps; + = off side for RH) for a line + arm. */
	static double TargetLineM(ECricketLine Line, ECricketBowlingArm Arm);

	/** Release speed (m/s) for a pace dial across an action's pace range, by family. */
	static double ResolveReleaseSpeedMS(const FCricketBowlingIntent& Intent, const FCricketBowlingAction& Action);
};
