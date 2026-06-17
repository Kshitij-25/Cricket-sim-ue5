#include "CricketTrajectoryPredictor.h"
#include "CricketBallIntegrator.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

FCricketTrajectoryPrediction FCricketTrajectoryPredictor::Predict(
	const FCricketBallState& InitialState,
	FCricketBallIntegrator Integrator,
	const FCricketPredictionParams& Params)
{
	FCricketTrajectoryPrediction Result;

	FCricketBallState S = InitialState;
	const double Dt = FMath::Max(Params.SampleInterval, 1.0e-4);
	const double ContactZ = Params.PitchPlaneZ + BallRadiusM;
	int32 Bounces = 0;

	// Record the starting point.
	Result.Samples.Add({0.0, S.Position, S.Velocity});

	double T = 0.0;
	while (T < Params.MaxTime)
	{
		const double PrevZ = S.Position.Z;
		Integrator.Advance(S, Dt);
		T += Dt;

		// Predicted bounce: centre crossed the contact height while descending.
		if (S.Velocity.Z < 0.0 && S.Position.Z <= ContactZ && PrevZ > ContactZ)
		{
			// Snap to the contact height (linear estimate within the step).
			const double Denom = PrevZ - S.Position.Z;
			const double Alpha = Denom > KINDA_SMALL_NUMBER ? (PrevZ - ContactZ) / Denom : 0.0;
			S.Position.Z = ContactZ;
			Result.BouncePoints.Add(S.Position);

			if (Params.bResolveBounces)
			{
				FCricketImpact Impact;
				Impact.ContactNormal = FVector(0, 0, 1);
				Impact.Variance = 0.0; // deterministic prediction
				const double SeamDotN = FMath::Abs(FVector::DotProduct(
					S.SeamNormal.GetSafeNormal(), Impact.ContactNormal));
				Impact.SeamContact = SeamDotN;
				FCricketPitchInteraction::ResolveBounce(S, Params.PitchPatch, Impact);
			}
			else
			{
				break; // first bounce only
			}

			if (++Bounces > Params.MaxBounces)
			{
				break;
			}
			(void)Alpha; // reserved for higher-order contact-time refinement
		}

		Result.Samples.Add({T, S.Position, S.Velocity});
	}

	Result.bReachedTimeLimit = (T >= Params.MaxTime);
	return Result;
}
