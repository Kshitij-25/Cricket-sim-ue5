#include "CricketSeamSolver.h"
#include "CricketPitchInteraction.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

void FCricketSeamSolver::Solve(
	const FCricketBallState& State,
	const FCricketSurfacePatch& Patch,
	const FCricketImpact& Impact,
	FCricketBounceContext& Ctx,
	FCricketBounceReport& Report)
{
	if (Impact.SeamContact <= 0.0)
	{
		return; // landed on the smooth leather — no seam movement
	}

	// Project the seam normal onto the pitch plane; that direction is the line
	// the seam tries to deflect the ball along.
	const FVector SeamN = State.SeamNormal.GetSafeNormal();
	FVector SeamLateral = SeamN - FVector::DotProduct(SeamN, Ctx.N) * Ctx.N;
	const double SeamLatLen = SeamLateral.Size();
	if (SeamLatLen <= KINDA_SMALL_NUMBER)
	{
		return; // seam edge-on to the surface — nothing to grip sideways
	}
	SeamLateral /= SeamLatLen;

	// Surface seam-grip: grassy + firm + lightly abrasive surfaces seam most.
	const double SeamGrip = Patch.Friction
		* (1.0 + 0.8 * Patch.GrassCoverage)
		* (1.0 + 0.3 * Patch.Roughness)
		* (0.7 + 0.3 * Patch.Hardness);

	// Wobble seam: a scrambled seam (low stability) wobbles the deviation
	// direction toward the cross-line and makes the magnitude inconsistent, both
	// driven deterministically by Variance so a given landing still repeats.
	const double Scramble = 1.0 - FMath::Clamp(State.SeamStability, 0.0, 1.0);
	if (Scramble > 0.0)
	{
		const FVector HorizIn = FVector(Ctx.VTangent.X, Ctx.VTangent.Y, 0.0).GetSafeNormal();
		if (!HorizIn.IsNearlyZero())
		{
			const FVector CrossLine = FVector::CrossProduct(FVector(0, 0, 1), HorizIn);
			SeamLateral = (SeamLateral + CrossLine * (Scramble * Impact.Variance)).GetSafeNormal(
				KINDA_SMALL_NUMBER, SeamLateral);
		}
	}

	// Up to ~12% of incoming speed redirected on a flush, grippy seam strike;
	// Variance is the luck of striking the seam square (and wobble inconsistency).
	const double WobbleMag = 1.0 + 0.4 * Scramble * Impact.Variance;
	const double SeamDeviation = 0.12 * Ctx.InSpeed * Impact.SeamContact * SeamGrip
		* (1.0 + 0.5 * Impact.Variance) * WobbleMag;

	Ctx.SeamImpulse = SeamLateral * SeamDeviation;

	// Report the signed cross-line share so it is comparable to the turn term.
	const FVector HorizIn = FVector(Ctx.VTangent.X, Ctx.VTangent.Y, 0.0).GetSafeNormal();
	if (!HorizIn.IsNearlyZero())
	{
		const FVector CrossLine = FVector::CrossProduct(FVector(0, 0, 1), HorizIn);
		Report.SeamDeviationMS = FVector::DotProduct(Ctx.SeamImpulse, CrossLine);
	}
}
