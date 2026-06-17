#include "CricketFieldingPredictor.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketBallIntegrator.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

namespace
{
	FORCEINLINE double Horiz2D(const FVector& A, const FVector& B)
	{
		return FVector(A.X - B.X, A.Y - B.Y, 0.0).Size();
	}
}

FVector FCricketBallPrediction::PositionAtTime(double T) const
{
	if (Path.Num() == 0) { return FVector::ZeroVector; }
	if (T <= Path[0].Time) { return Path[0].Position; }
	if (T >= Path.Last().Time) { return Path.Last().Position; }
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		if (Path[i].Time >= T)
		{
			const double Span = Path[i].Time - Path[i - 1].Time;
			const double A = Span > KINDA_SMALL_NUMBER ? (T - Path[i - 1].Time) / Span : 0.0;
			return FMath::Lerp(Path[i - 1].Position, Path[i].Position, A);
		}
	}
	return Path.Last().Position;
}

FVector FCricketBallPrediction::VelocityAtTime(double T) const
{
	if (Path.Num() == 0) { return FVector::ZeroVector; }
	if (T <= Path[0].Time) { return Path[0].Velocity; }
	if (T >= Path.Last().Time) { return Path.Last().Velocity; }
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		if (Path[i].Time >= T)
		{
			const double Span = Path[i].Time - Path[i - 1].Time;
			const double A = Span > KINDA_SMALL_NUMBER ? (T - Path[i - 1].Time) / Span : 0.0;
			return FMath::Lerp(Path[i - 1].Velocity, Path[i].Velocity, A);
		}
	}
	return Path.Last().Velocity;
}

FCricketBallPrediction FCricketFieldingPredictor::PredictBall(
	const FCricketBallState& BallState,
	FCricketBallIntegrator Integrator,
	const FCricketPredictionParams& Params)
{
	FCricketBallPrediction Out;

	const FCricketTrajectoryPrediction Traj =
		FCricketTrajectoryPredictor::Predict(BallState, MoveTemp(Integrator), Params);

	Out.Path = Traj.Samples;
	if (Out.Path.Num() == 0) { return Out; }
	Out.bValid = true;
	Out.FlightTimeSec = Out.Path.Last().Time;

	const double GroundContactZ = Params.PitchPlaneZ + BallRadiusM;

	// Apex: the highest sample. Tracked across the whole window.
	Out.ApexHeightM = -TNumericLimits<double>::Max();

	// First descending ground crossing => the landing point & the catch/ground split.
	bool bFoundBounce = false;
	for (int32 i = 0; i < Out.Path.Num(); ++i)
	{
		const FCricketTrajectorySample& S = Out.Path[i];
		if (S.Position.Z > Out.ApexHeightM)
		{
			Out.ApexHeightM = S.Position.Z;
			Out.ApexM = S.Position;
			Out.TimeToApexSec = S.Time;
		}
		if (!bFoundBounce && i > 0)
		{
			// The predictor snaps the bounce sample to the contact height and reflects
			// its velocity, so detect the landing by the Z crossing alone (the
			// post-bounce velocity sign is already flipped on this very sample).
			const FCricketTrajectorySample& P = Out.Path[i - 1];
			if (P.Position.Z > GroundContactZ && S.Position.Z <= GroundContactZ + 1e-6)
			{
				bFoundBounce = true;
				Out.bWillBounce = true;
				Out.LandingPointM = S.Position;
				Out.TimeToLandSec = S.Time;
				Out.FirstBounceTimeSec = S.Time;
			}
		}
	}

	// Fallback: if the scan missed it but the predictor logged a bounce, use that.
	if (!bFoundBounce && Traj.BouncePoints.Num() > 0)
	{
		Out.bWillBounce = true;
		Out.LandingPointM = Traj.BouncePoints[0];
		Out.TimeToLandSec = Out.FlightTimeSec;
		Out.FirstBounceTimeSec = Out.FlightTimeSec;
	}

	if (!Out.bWillBounce)
	{
		// Never came down in the window (still climbing / long carry): treat the
		// end of the window as the split so everything counts as an in-air chance.
		Out.FirstBounceTimeSec = Out.FlightTimeSec;
		Out.LandingPointM = Out.Path.Last().Position;
		Out.TimeToLandSec = Out.FlightTimeSec;
	}
	return Out;
}

FCricketInterceptResult FCricketFieldingPredictor::SolveIntercept(
	const FCricketBallPrediction& Prediction,
	const FCricketInterceptQuery& Query)
{
	FCricketInterceptResult Out;
	if (!Prediction.bValid || Prediction.Path.Num() == 0)
	{
		return Out;
	}

	const double MaxSpeed = FMath::Max(Query.MaxSpeedMS, 0.1);

	for (const FCricketTrajectorySample& S : Prediction.Path)
	{
		const bool bAirborne = S.Time < Prediction.FirstBounceTimeSec - KINDA_SMALL_NUMBER;
		const bool bHeightCatchable = (S.Position.Z >= Query.CatchReachLowM && S.Position.Z <= Query.CatchReachHeightM);
		const bool bGroundReachable = (S.Position.Z <= Query.GroundFieldHeightM);

		const bool bCatchSample  = bAirborne && bHeightCatchable;
		const bool bGroundSample = !bAirborne && bGroundReachable;
		if (!bCatchSample && !bGroundSample)
		{
			continue; // ball overhead-out-of-reach or still in the air past us: wait
		}

		const double HorizDist = Horiz2D(Query.FielderPosM, S.Position);
		const double AvailTime = S.Time - Query.ReactionTimeSec;
		const double MaxRun = MaxSpeed * FMath::Max(AvailTime, 0.0);

		// Reachable if a top-speed run (after reacting) gets within the reach radius.
		if (HorizDist > MaxRun + Query.ReachRadiusM + 1e-6)
		{
			continue; // not at this sample — but a later sample may still work
		}

		// We can make it. Fill the result for the earliest such sample.
		const double NeededRun = FMath::Max(0.0, HorizDist - Query.ReachRadiusM);
		Out.bCanIntercept = true;
		Out.Kind = bCatchSample ? ECricketInterceptKind::Catch : ECricketInterceptKind::GroundField;
		Out.PointM = S.Position;
		Out.TimeSec = S.Time;
		Out.DistanceM = HorizDist;
		Out.RequiredSpeedMS = (AvailTime > 1e-3) ? (NeededRun / AvailTime) : (NeededRun > 0.0 ? TNumericLimits<double>::Max() : 0.0);
		Out.SlackSec = AvailTime - (NeededRun / MaxSpeed);

		// Difficulty from geometry: a dive (using the reach radius) is hardest; a
		// comfortable run with slack is regulation; in between is on the move.
		if (HorizDist > MaxRun) // only reachable by stretching/diving
		{
			Out.Difficulty = ECricketCatchDifficulty::Diving;
		}
		else if (Out.SlackSec < 0.3)
		{
			Out.Difficulty = ECricketCatchDifficulty::Running;
		}
		else
		{
			Out.Difficulty = ECricketCatchDifficulty::Regulation;
		}
		return Out;
	}

	// Never reachable in the window: a boundary, or simply out of range.
	Out.bCanIntercept = false;
	Out.Kind = ECricketInterceptKind::None;
	Out.Difficulty = ECricketCatchDifficulty::Impossible;
	return Out;
}

FCricketThrowSolution FCricketFieldingPredictor::SolveThrow(
	const FVector& FromM, const FVector& TargetM, double LaunchSpeedMS, bool bPreferFlat)
{
	FCricketThrowSolution Out;

	const FVector Delta = TargetM - FromM;
	const FVector Horiz(Delta.X, Delta.Y, 0.0);
	const double R = Horiz.Size();
	const double H = Delta.Z;
	const double V = FMath::Max(LaunchSpeedMS, 0.1);
	const double G = GravityMS2;

	// Near-vertical throw (no horizontal range): only feasible straight up.
	if (R < KINDA_SMALL_NUMBER)
	{
		if (V * V >= 2.0 * G * FMath::Max(H, 0.0))
		{
			Out.bFeasible = true;
			Out.LaunchVelocityMS = FVector(0, 0, V);
			Out.FlightTimeSec = (V - FMath::Sqrt(FMath::Max(V * V - 2.0 * G * H, 0.0))) / G;
			Out.LaunchElevationDeg = 90.0;
		}
		return Out;
	}

	const FVector Dir = Horiz / R;

	// Projectile range equation: solve tanθ from v, R, h, g.
	const double Disc = V * V * V * V - G * (G * R * R + 2.0 * H * V * V);
	if (Disc < 0.0)
	{
		return Out; // out of range for this speed
	}

	const double SqrtD = FMath::Sqrt(Disc);
	const double TanLow  = (V * V - SqrtD) / (G * R);
	const double TanHigh = (V * V + SqrtD) / (G * R);
	const double TanTheta = bPreferFlat ? TanLow : TanHigh;
	const double Theta = FMath::Atan(TanTheta);

	const double VHoriz = V * FMath::Cos(Theta);
	const double VUp = V * FMath::Sin(Theta);

	Out.bFeasible = true;
	Out.LaunchVelocityMS = Dir * VHoriz + FVector(0, 0, VUp);
	Out.FlightTimeSec = (VHoriz > KINDA_SMALL_NUMBER) ? (R / VHoriz) : 0.0;
	Out.LaunchElevationDeg = FMath::RadiansToDegrees(Theta);
	return Out;
}
