#include "CricketPitchInteraction.h"
#include "CricketBounceSolver.h"
#include "CricketSpinSolver.h"
#include "CricketSeamSolver.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

FCricketBounceReport FCricketPitchInteraction::ResolveBounce(
	FCricketBallState& State,
	const FCricketSurfacePatch& Patch,
	const FCricketImpact& Impact)
{
	FCricketBounceReport Report;

	// --- Decompose the incoming state once (shared by all three solvers) ----
	FCricketBounceContext Ctx;
	Ctx.N = Impact.ContactNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 0, 1));
	Ctx.Vn = FVector::DotProduct(State.Velocity, Ctx.N); // signed; negative = approaching

	// Only resolve an actual incoming contact.
	if (Ctx.Vn >= 0.0)
	{
		return Report;
	}

	Ctx.InVelocity = State.Velocity;
	Ctx.InSpeed = State.Velocity.Size();
	Ctx.VNormal = Ctx.Vn * Ctx.N;               // normal component
	Ctx.VTangent = State.Velocity - Ctx.VNormal; // tangential component
	Ctx.ImpactSpeed = FMath::Abs(Ctx.Vn);

	// --- Run the three solvers in order -------------------------------------
	// 1) Normal response sets restitution and the normal impulse the friction
	//    cone (2) depends on; 3) seam movement is independent of both.
	FCricketBounceSolver::Solve(State, Patch, Impact, Ctx, Report);
	FCricketSpinSolver::Solve(State, Patch, Impact, Ctx, Report);
	FCricketSeamSolver::Solve(State, Patch, Impact, Ctx, Report);

	// --- Assemble the post-bounce velocity ----------------------------------
	const FVector NewVelocity =
		Ctx.NewVNormal + Ctx.VTangent + Ctx.TangImpulse + Ctx.SeamImpulse;
	State.Velocity = NewVelocity;

	// --- Finalise the report ------------------------------------------------
	const double OutSpeed = State.Velocity.Size();
	Report.SpeedRetainedFrac = Ctx.InSpeed > KINDA_SMALL_NUMBER ? OutSpeed / Ctx.InSpeed : 0.0;

	// Total lateral (cross-line) deviation = turn (spin) + seam movement.
	Report.LateralDeviationMS = Report.TurnMS + Report.SeamDeviationMS;

	// Outgoing angle above horizontal — the BOUNCE ANGLE.
	const double OutHoriz = FVector(State.Velocity.X, State.Velocity.Y, 0.0).Size();
	const double OutVert = State.Velocity.Z;
	Report.BounceAngleDeg = FMath::RadiansToDegrees(
		FMath::Atan2(OutVert, FMath::Max(OutHoriz, KINDA_SMALL_NUMBER)));

	return Report;
}
