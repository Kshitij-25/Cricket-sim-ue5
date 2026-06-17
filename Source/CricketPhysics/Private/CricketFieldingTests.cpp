// Headless automation tests for the fielding prediction core: real ball forecasts,
// fielder reachability (catch vs ground field vs boundary), and ballistic throws.
// Every test runs the REAL physics predictor — no scripted landing points.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Fielding; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketFieldingPredictor.h"
#include "CricketFieldingTypes.h"
#include "CricketBallIntegrator.h"
#include "CricketPhysicsTypes.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	FCricketBallIntegrator MakeIntegrator()
	{
		// Default ball/air: drag on, no swing/spin — a struck ball flying through air.
		return FCricketBallIntegrator();
	}

	FCricketBallState LaunchedBall(const FVector& PosM, const FVector& VelMS)
	{
		FCricketBallState B;
		B.Position = PosM;
		B.Velocity = VelMS;
		return B;
	}

	FCricketPredictionParams Params(double MaxTime = 6.0)
	{
		FCricketPredictionParams P;
		P.MaxTime = MaxTime;
		P.SampleInterval = 0.01;
		P.PitchPlaneZ = 0.0;
		P.MaxBounces = 3;
		P.bResolveBounces = true;
		return P;
	}

	FCricketInterceptQuery FielderAt(const FVector& PosM)
	{
		FCricketInterceptQuery Q;
		Q.FielderPosM = PosM;
		return Q;
	}
}

// 1. GROUND BALL: a flat, hard shot stays low, bounces, and is fielded off the
//    ground by someone in its path — never caught.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingGroundBallTest,
	"CricketSim.Fielding.GroundBall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingGroundBallTest::RunTest(const FString&)
{
	const FCricketBallPrediction Pred = FCricketFieldingPredictor::PredictBall(
		LaunchedBall(FVector(0, 0, 0.4), FVector(24, 0, 0.5)), MakeIntegrator(), Params(3.0));

	TestTrue(TEXT("Prediction valid"), Pred.bValid);
	TestTrue(TEXT("Ground ball bounces"), Pred.bWillBounce);
	TestTrue(TEXT("Stays low (apex below head height)"), Pred.ApexHeightM < 1.5);

	// A fielder square in its path picks it up off the ground.
	const FCricketInterceptResult R = FCricketFieldingPredictor::SolveIntercept(Pred, FielderAt(FVector(18, 0, 0)));
	TestTrue(TEXT("Fielder in the path intercepts"), R.bCanIntercept);
	TestEqual(TEXT("It is a ground field, not a catch"), R.Kind, ECricketInterceptKind::GroundField);

	// Someone 25 m to the side cannot get there.
	const FCricketInterceptResult Far = FCricketFieldingPredictor::SolveIntercept(Pred, FielderAt(FVector(18, 25, 0)));
	TestFalse(TEXT("Distant fielder cannot reach a ground ball"), Far.bCanIntercept);
	return true;
}

// 2. LOFTED SHOT: an aerial drive arcs up and comes down; a fielder under where it
//    lands takes it as a catch (in the air, before the bounce).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingLoftedTest,
	"CricketSim.Fielding.LoftedShot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingLoftedTest::RunTest(const FString&)
{
	const FCricketBallPrediction Pred = FCricketFieldingPredictor::PredictBall(
		LaunchedBall(FVector(0, 0, 1.0), FVector(18, 0, 12)), MakeIntegrator(), Params(6.0));

	TestTrue(TEXT("Lofted shot climbs"), Pred.ApexHeightM > 4.0);
	TestTrue(TEXT("And comes down"), Pred.bWillBounce);
	TestTrue(TEXT("Carries downfield"), Pred.LandingPointM.X > 15.0);

	const FVector Under(Pred.LandingPointM.X, Pred.LandingPointM.Y, 0.0);
	const FCricketInterceptResult R = FCricketFieldingPredictor::SolveIntercept(Pred, FielderAt(Under));
	TestTrue(TEXT("Fielder under it can take it"), R.bCanIntercept);
	TestEqual(TEXT("Taken as a catch"), R.Kind, ECricketInterceptKind::Catch);
	return true;
}

// 3. HIGH CATCH: a steep skyer hangs a long time and very high; the fielder has
//    plenty of time to settle under it (a regulation high catch).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingHighCatchTest,
	"CricketSim.Fielding.HighCatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingHighCatchTest::RunTest(const FString&)
{
	const FCricketBallPrediction Pred = FCricketFieldingPredictor::PredictBall(
		LaunchedBall(FVector(0, 0, 1.0), FVector(7, 0, 22)), MakeIntegrator(), Params(7.0));

	TestTrue(TEXT("Skyer goes very high"), Pred.ApexHeightM > 10.0);
	TestTrue(TEXT("Long hang time"), Pred.TimeToLandSec > 2.5);

	const FVector Under(Pred.LandingPointM.X, Pred.LandingPointM.Y, 0.0);
	const FCricketInterceptResult R = FCricketFieldingPredictor::SolveIntercept(Pred, FielderAt(Under));
	TestTrue(TEXT("High catch is takeable"), R.bCanIntercept);
	TestEqual(TEXT("In the air"), R.Kind, ECricketInterceptKind::Catch);
	TestTrue(TEXT("Comfortable: positive time slack"), R.SlackSec > 0.0);
	return true;
}

// 4. BOUNDARY SAVE vs FOUR: a fielder near the line who is in range saves it; one
//    out of range does not — the ball would beat them (a boundary).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingBoundaryTest,
	"CricketSim.Fielding.BoundarySave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingBoundaryTest::RunTest(const FString&)
{
	// A hard, flattish shot racing toward the rope.
	const FCricketBallPrediction Pred = FCricketFieldingPredictor::PredictBall(
		LaunchedBall(FVector(0, 0, 0.6), FVector(30, 0, 4)), MakeIntegrator(), Params(5.0));
	TestTrue(TEXT("Prediction valid"), Pred.bValid);

	// Find a point well downfield on the ball's line to station fielders near.
	const FVector Late = Pred.PositionAtTime(1.4);

	// In range: a stride off the line, with time — saves it.
	const FCricketInterceptResult Saver =
		FCricketFieldingPredictor::SolveIntercept(Pred, FielderAt(FVector(Late.X - 6.0, 1.0, 0.0)));
	TestTrue(TEXT("In-range fielder saves the boundary"), Saver.bCanIntercept);

	// Out of range: far off the line — the ball beats them to the rope.
	const FCricketInterceptResult Beaten =
		FCricketFieldingPredictor::SolveIntercept(Pred, FielderAt(FVector(Late.X, 40.0, 0.0)));
	TestFalse(TEXT("Out-of-range fielder cannot prevent the four"), Beaten.bCanIntercept);
	return true;
}

// 5. DIRECT HIT (throw solver exactness): the ballistic aim, flown under gravity,
//    lands exactly on the stumps; an out-of-range target is reported infeasible.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingDirectHitTest,
	"CricketSim.Fielding.DirectHit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingDirectHitTest::RunTest(const FString&)
{
	const FVector From(10, -8, 1.5);     // fielder release point
	const FVector Stumps(28, 2, 0.3);    // base of the stumps
	const double Speed = 28.0;

	const FCricketThrowSolution Sol = FCricketFieldingPredictor::SolveThrow(From, Stumps, Speed, /*flat*/ true);
	TestTrue(TEXT("Throw is feasible"), Sol.bFeasible);
	TestTrue(TEXT("Flat run-out throw is low"), Sol.LaunchElevationDeg < 30.0);

	// Fly the analytic solution under gravity only; it must arrive on the stumps.
	const double T = Sol.FlightTimeSec;
	const FVector Landed = From + Sol.LaunchVelocityMS * T - FVector(0, 0, 0.5 * GravityMS2 * T * T);
	TestTrue(TEXT("Direct hit lands on the stumps"), FVector::Dist(Landed, Stumps) < 0.02);

	// A target far beyond range for the speed is infeasible (not a silent miss).
	const FCricketThrowSolution TooFar = FCricketFieldingPredictor::SolveThrow(From, FVector(200, 0, 0.3), Speed, true);
	TestFalse(TEXT("Out-of-range target reported infeasible"), TooFar.bFeasible);
	return true;
}

// 6. THROW THROUGH REAL PHYSICS: the aimed throw, flown by the actual integrator,
//    still passes close to the target — drag is a small correction, not a rewrite.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingThrowPhysicsTest,
	"CricketSim.Fielding.ThrowReachesTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingThrowPhysicsTest::RunTest(const FString&)
{
	const FVector From(5, 0, 1.6);
	const FVector Target(22, 0, 0.4);
	const FCricketThrowSolution Sol = FCricketFieldingPredictor::SolveThrow(From, Target, 30.0, true);
	TestTrue(TEXT("Throw feasible"), Sol.bFeasible);

	// Fly the thrown ball through the full model and find its closest approach.
	FCricketPredictionParams P = Params(2.0);
	P.bResolveBounces = false; // we care about the in-flight pass at the stumps
	const FCricketBallPrediction Flight = FCricketFieldingPredictor::PredictBall(
		LaunchedBall(From, Sol.LaunchVelocityMS), MakeIntegrator(), P);

	double MinDist = TNumericLimits<double>::Max();
	for (const FCricketTrajectorySample& S : Flight.Path)
	{
		MinDist = FMath::Min(MinDist, FVector::Dist(S.Position, Target));
	}
	TestTrue(TEXT("Thrown ball passes close to the target under real drag"), MinDist < 1.0);
	return true;
}

// 7. DETERMINISM: the same launch predicts the same flight (no RNG anywhere).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketFieldingDeterminismTest,
	"CricketSim.Fielding.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketFieldingDeterminismTest::RunTest(const FString&)
{
	const FVector Pos(0, 0, 1.0), Vel(16, 5, 11);
	const FCricketBallPrediction A = FCricketFieldingPredictor::PredictBall(LaunchedBall(Pos, Vel), MakeIntegrator(), Params());
	const FCricketBallPrediction B = FCricketFieldingPredictor::PredictBall(LaunchedBall(Pos, Vel), MakeIntegrator(), Params());

	TestEqual(TEXT("Same sample count"), A.Path.Num(), B.Path.Num());
	TestTrue(TEXT("Identical landing point"), A.LandingPointM == B.LandingPointM);
	TestTrue(TEXT("Identical apex"), A.ApexM == B.ApexM);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
