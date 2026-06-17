#include "CricketBounceSolver.h"
#include "CricketPitchInteraction.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

void FCricketBounceSolver::Solve(
	const FCricketBallState& /*State*/,
	const FCricketSurfacePatch& Patch,
	const FCricketImpact& Impact,
	FCricketBounceContext& Ctx,
	FCricketBounceReport& Report)
{
	// --- Restitution -------------------------------------------------------
	// Base coefficient is the surface's intrinsic bounciness; hardness makes it
	// springier, moisture and a thick grass top deaden it, and faster impacts
	// lose more energy as the ball deforms and the turf yields.
	const double SpeedFalloff = 1.0 / (1.0 + 0.012 * Ctx.ImpactSpeed); // ~ -1.2% per m/s
	double e = Patch.Restitution
		* (0.75 + 0.35 * Patch.Hardness)
		* (1.0 - 0.45 * Patch.Moisture)
		* (1.0 - 0.20 * Patch.GrassCoverage)
		* SpeedFalloff;

	// Deterministic bounce variation from cracks/footmarks (the inverse of
	// bounce consistency). Same spot + same Variance => same kick, every time.
	e *= 1.0 + 0.35 * Patch.Unevenness * Impact.Variance;
	e = FMath::Clamp(e, 0.0, 0.95);

	// --- Normal response ---------------------------------------------------
	Ctx.Restitution = e;
	Ctx.NewVNormal = -e * Ctx.VNormal;        // reflect normal component
	Ctx.Jn = (1.0 + e) * Ctx.ImpactSpeed;     // normal impulse (delta-v units)

	// --- Report: incoming angle + ballistic apex of the rebound ------------
	Report.RestitutionUsed = e;
	const double HorizIn = FVector(Ctx.VTangent.X, Ctx.VTangent.Y, 0.0).Size();
	Report.IncomingAngleDeg = FMath::RadiansToDegrees(
		FMath::Atan2(Ctx.ImpactSpeed, FMath::Max(HorizIn, KINDA_SMALL_NUMBER)));

	// Apex height of the rebound from the normal velocity alone (aero ignored):
	// h = (e*Vn * Nz)^2 / 2g, using the vertical share of the rebound normal.
	const double ReboundVn = e * Ctx.ImpactSpeed;
	const double VerticalUp = ReboundVn * FMath::Max(Ctx.N.Z, 0.0);
	Report.BounceHeightM = (VerticalUp * VerticalUp) / (2.0 * GravityMS2);
}
