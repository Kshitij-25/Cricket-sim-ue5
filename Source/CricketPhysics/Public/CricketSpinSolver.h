#pragma once

#include "CoreMinimal.h"

struct FCricketBallState;
struct FCricketSurfacePatch;
struct FCricketImpact;
struct FCricketBounceContext;
struct FCricketBounceReport;

/**
 * FCricketSpinSolver — the TANGENTIAL response of a bounce: friction, the
 * grip/skid decision, and spin->translation coupling (this is the TURN).
 *
 * It builds the velocity of the material point in contact (which folds the spin
 * in via omega x r), then applies Coulomb friction:
 *   - GRIP: if the impulse needed to arrest the slip is within the friction
 *           cone, the ball bites and its spin converts to sideways translation
 *           (off/leg-break turn; topspin->extra dig; backspin->skid-on).
 *   - SKID: otherwise it slides on, taking only the Coulomb-limited impulse.
 *
 * The effective friction is the patch's base coefficient modulated by the
 * surface state — abrasive/worn/dry tracks GRIP harder (more turn); wet/greasy
 * and lush-grass tops SKID (pace on, less turn):
 *   mu = Friction * (1+0.6*Roughness) * (1+0.5*Wear)
 *                 * (1-0.35*Moisture) * (1-0.25*GrassCoverage)
 *
 * Mutates State.AngularVelocity (the spin is consumed by the grip) and writes
 * Context.TangImpulse plus the grip/turn fields of the report.
 */
class CRICKETPHYSICS_API FCricketSpinSolver
{
public:
	static void Solve(
		FCricketBallState& State,
		const FCricketSurfacePatch& Patch,
		const FCricketImpact& Impact,
		FCricketBounceContext& Context,
		FCricketBounceReport& Report);
};
