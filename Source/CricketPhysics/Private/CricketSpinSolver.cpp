#include "CricketSpinSolver.h"
#include "CricketPitchInteraction.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

void FCricketSpinSolver::Solve(
	FCricketBallState& State,
	const FCricketSurfacePatch& Patch,
	const FCricketImpact& /*Impact*/,
	FCricketBounceContext& Ctx,
	FCricketBounceReport& Report)
{
	// --- Contact-point velocity (couples spin into the bounce) -------------
	// The material point in contact sits at -R*N from the centre, so it carries
	// the spin term omega x r in addition to the tangential CoM velocity.
	const FVector RContact = -BallRadiusM * Ctx.N;
	const FVector ContactVel = Ctx.VTangent + FVector::CrossProduct(State.AngularVelocity, RContact);
	const double ContactTangSpeed = ContactVel.Size();

	// --- Effective friction from the surface state -------------------------
	// Abrasive, worn, dry surfaces bite (more turn); wet/greasy and lush-grass
	// tops let the ball skid on (pace, less turn).
	double Mu = Patch.Friction
		* (1.0 + 0.6 * Patch.Roughness)
		* (1.0 + 0.5 * Patch.Wear)
		* (1.0 - 0.35 * Patch.Moisture)
		* (1.0 - 0.25 * Patch.GrassCoverage);
	Mu = FMath::Max(Mu, 0.0);
	Report.FrictionUsed = Mu;

	// Impulse needed to fully arrest sliding at the contact point. The reduced
	// mass term k = 1 + m R^2 / I accounts for the rotational coupling.
	const double K = 1.0 + (BallMassKg * BallRadiusM * BallRadiusM) / BallInertia; // = 2.5
	const double JtStick = ContactTangSpeed / K;   // impulse (delta-v) to stop slip
	const double JtMax = Mu * Ctx.Jn;              // Coulomb limit

	FVector TangImpulse = FVector::ZeroVector;
	if (ContactTangSpeed > KINDA_SMALL_NUMBER)
	{
		const FVector TangDir = ContactVel / ContactTangSpeed;
		if (JtStick <= JtMax)
		{
			// GRIP: the ball bites — spin converts to translation (this is TURN).
			TangImpulse = -JtStick * TangDir;
			Report.bGripped = true;
			Report.GripLevel = 1.0;
		}
		else
		{
			// SKID: slides on, only the Coulomb-limited impulse is applied.
			TangImpulse = -JtMax * TangDir;
			Report.bGripped = false;
			Report.GripLevel = JtStick > KINDA_SMALL_NUMBER
				? FMath::Clamp(JtMax / JtStick, 0.0, 1.0) : 0.0;
		}
	}
	else
	{
		Report.GripLevel = 1.0; // nothing to slip => trivially "gripped"
	}

	Ctx.TangImpulse = TangImpulse;

	// --- Angular response to the tangential impulse ------------------------
	// dOmega = (RContact x J)/I, with J converted back to impulse units (x mass).
	const FVector AngularImpulse =
		FVector::CrossProduct(RContact, TangImpulse * BallMassKg) / BallInertia;
	State.AngularVelocity += AngularImpulse;

	// --- Report: turn = the cross-line share of the friction impulse -------
	// (Measured against the original horizontal flight line so it is comparable
	// across pitches; the orchestrator combines it with the seam contribution.)
	const FVector HorizIn = FVector(Ctx.VTangent.X, Ctx.VTangent.Y, 0.0).GetSafeNormal();
	if (!HorizIn.IsNearlyZero())
	{
		const FVector CrossLine = FVector::CrossProduct(FVector(0, 0, 1), HorizIn);
		Report.TurnMS = FVector::DotProduct(TangImpulse, CrossLine);
	}
}
