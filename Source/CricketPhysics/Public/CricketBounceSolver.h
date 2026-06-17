#pragma once

#include "CoreMinimal.h"

struct FCricketBallState;
struct FCricketSurfacePatch;
struct FCricketImpact;
struct FCricketBounceContext;
struct FCricketBounceReport;

/**
 * FCricketBounceSolver — the NORMAL response of a bounce.
 *
 * Computes the effective restitution from the surface (hardness, restitution
 * coefficient, moisture, grass) with a speed-dependent fall-off and a
 * deterministic consistency perturbation, then reflects the normal velocity.
 * Writes Context.Restitution / Jn / NewVNormal and the bounce angle/height into
 * the report. Knows nothing about spin or seam — those are separate solvers.
 *
 * The model:
 *   e  = Restitution
 *      * (0.75 + 0.35*Hardness)      // firm surfaces are springier
 *      * (1 - 0.45*Moisture)         // wet deadens
 *      * (1 - 0.20*GrassCoverage)    // a thick green top can cushion
 *      * 1/(1 + 0.012*ImpactSpeed)   // faster impacts lose more energy
 *      * (1 + 0.35*Unevenness*Variance)   // deterministic bounce variation
 *   clamped to [0, 0.95].
 */
class CRICKETPHYSICS_API FCricketBounceSolver
{
public:
	static void Solve(
		const FCricketBallState& State,
		const FCricketSurfacePatch& Patch,
		const FCricketImpact& Impact,
		FCricketBounceContext& Context,
		FCricketBounceReport& Report);
};
