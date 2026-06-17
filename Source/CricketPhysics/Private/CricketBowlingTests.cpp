// Headless automation tests for the bowling system. They assert that the
// FCricketDeliveryGenerator turns intent into release parameters whose EMERGENT
// flight (through the same shared model the live ball uses) matches the named
// delivery: yorkers pitch full, bouncers rear up, swing goes the right way,
// reverse flips, and the spinners turn the correct direction off the pitch.
//   UnrealEditor-Cmd CricketSim.uproject -ExecCmds="Automation RunTests CricketSim.Bowling" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketDeliveryGenerator.h"
#include "CricketBowlingActionAsset.h"
#include "CricketBallIntegrator.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketPitchInteraction.h"
#include "CricketAerodynamics.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	const double StrikerX = CricketField::DefaultReleaseToStumpsM;

	FCricketDeliveryContext MakeContext(double ReleaseHeightM = 2.1)
	{
		FCricketDeliveryContext C;
		C.ReleasePositionM = FVector(0.0, 0.0, ReleaseHeightM);
		C.StrikerStumpsM   = FVector(StrikerX, 0.0, 0.0);
		C.Seed = 0;
		C.HumanScatter = 0.0;
		return C;
	}

	FCricketBowlingIntent MakeIntent(ECricketMovement Movement, ECricketLength Length, ECricketLine Line)
	{
		FCricketBowlingIntent In;
		In.Movement = Movement;
		In.Length = Length;
		In.Line = Line;
		return In;
	}

	FCricketBallState StateFromParams(const FCricketReleaseParameters& P)
	{
		FCricketBallState S;
		S.Position        = P.ReleasePositionM;
		S.Velocity        = P.ReleaseVelocityMS;
		S.AngularVelocity = P.AngularVelocityRadS;
		S.SeamNormal      = P.SeamNormal;
		S.SeamStability   = P.SeamStability;
		return S;
	}

	// Advance to the moment of first bounce; leaves S as the PRE-bounce state.
	bool IntegrateToBounce(FCricketBallState& S, const FCricketBallSurface& Surf,
		const FCricketEnvironment& Env, const FCricketAeroCoefficients& C)
	{
		FCricketBallIntegrator I(Surf, Env, C);
		const double ContactZ = BallRadiusM;
		for (int32 i = 0; i < 4000; ++i)
		{
			const double PrevZ = S.Position.Z;
			I.Advance(S, 0.001);
			if (S.Velocity.Z < 0.0 && S.Position.Z <= ContactZ && PrevZ > ContactZ)
			{
				S.Position.Z = ContactZ;
				return true;
			}
		}
		return false;
	}

	FCricketTrajectoryPrediction PredictFull(const FCricketReleaseParameters& P,
		const FCricketSurfacePatch& Patch, double MaxTime)
	{
		FCricketBallIntegrator I(P.BallCondition, FCricketEnvironment(), P.Coefficients);
		FCricketPredictionParams Pr;
		Pr.MaxTime = MaxTime;
		Pr.SampleInterval = 0.003;
		Pr.PitchPlaneZ = 0.0;
		Pr.bResolveBounces = true;
		Pr.MaxBounces = 3;
		Pr.PitchPatch = Patch;
		return FCricketTrajectoryPredictor::Predict(StateFromParams(P), I, Pr);
	}

	// Ball-centre height where the path first reaches world X (linear interp).
	double ZAtX(const FCricketTrajectoryPrediction& Pred, double X)
	{
		for (int32 i = 1; i < Pred.Samples.Num(); ++i)
		{
			const double X0 = Pred.Samples[i - 1].Position.X;
			const double X1 = Pred.Samples[i].Position.X;
			if ((X0 <= X && X <= X1) || (X1 <= X && X <= X0))
			{
				const double T = (FMath::Abs(X1 - X0) > KINDA_SMALL_NUMBER) ? (X - X0) / (X1 - X0) : 0.0;
				return FMath::Lerp(Pred.Samples[i - 1].Position.Z, Pred.Samples[i].Position.Z, T);
			}
		}
		return -1.0;
	}

	FCricketSurfacePatch HardPatch()
	{
		FCricketSurfacePatch P;
		P.Hardness = 0.8;
		P.Friction = 0.5;
		P.Moisture = 0.05;
		return P;
	}

	// Change in lateral velocity across a single bounce on a gripping pitch.
	double BounceTurnDeltaY(const FCricketReleaseParameters& P)
	{
		FCricketBallState S = StateFromParams(P);
		if (!IntegrateToBounce(S, P.BallCondition, FCricketEnvironment(), P.Coefficients))
		{
			return 0.0;
		}
		const double PreVy = S.Velocity.Y;

		FCricketSurfacePatch Grip;
		Grip.Hardness = 0.7;
		Grip.Friction = 0.6;
		FCricketImpact Impact;
		Impact.ContactNormal = FVector(0, 0, 1);
		Impact.SeamContact = FMath::Abs(FVector::DotProduct(S.SeamNormal.GetSafeNormal(), Impact.ContactNormal));
		Impact.Variance = 0.0;
		FCricketPitchInteraction::ResolveBounce(S, Grip, Impact);
		return S.Velocity.Y - PreVy;
	}
}

// 1. Generator determinism: identical inputs (and seed) reproduce identical params.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingDeterminismTest,
	"CricketSim.Bowling.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingDeterminismTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeExpressQuick();
	const FCricketBowlingIntent In = MakeIntent(ECricketMovement::SeamUp, ECricketLength::GoodLength, ECricketLine::OffStump);
	const FCricketDeliveryContext Ctx = MakeContext();

	const FCricketReleaseParameters P1 = FCricketDeliveryGenerator::Generate(In, A, Ctx);
	const FCricketReleaseParameters P2 = FCricketDeliveryGenerator::Generate(In, A, Ctx);

	TestTrue(TEXT("Release velocity is reproduced bit-for-bit"), P1.ReleaseVelocityMS.Equals(P2.ReleaseVelocityMS, 1e-9));
	TestTrue(TEXT("Seam normal is reproduced bit-for-bit"), P1.SeamNormal.Equals(P2.SeamNormal, 1e-9));
	TestTrue(TEXT("Spin is reproduced bit-for-bit"), P1.AngularVelocityRadS.Equals(P2.AngularVelocityRadS, 1e-9));
	TestEqual(TEXT("Release elevation is reproduced"), P1.ReleaseElevationDeg, P2.ReleaseElevationDeg);
	return true;
}

// 2. Yorker: pitches at the batter's feet (very full).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingYorkerTest,
	"CricketSim.Bowling.Yorker", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingYorkerTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeExpressQuick();
	FCricketBowlingIntent In = MakeIntent(ECricketMovement::SeamUp, ECricketLength::Yorker, ECricketLine::OffStump);
	In.Pace01 = 0.95;

	FCricketDeliveryDiagnostics D;
	FCricketDeliveryGenerator::Generate(In, A, MakeContext(2.1), &D);

	TestTrue(TEXT("Yorker pitches very full (< 2.5 m from the stumps)"), D.PredictedLengthM < 2.5);
	TestTrue(TEXT("Yorker still pitches in front of the stumps"), D.PredictedLengthM > -0.5);
	TestTrue(TEXT("Yorker aim solve converged"), D.bAimConverged);
	return true;
}

// 3. Good length pitches in the good-length band.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingGoodLengthTest,
	"CricketSim.Bowling.GoodLength", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingGoodLengthTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeExpressQuick();
	FCricketBowlingIntent In = MakeIntent(ECricketMovement::SeamUp, ECricketLength::GoodLength, ECricketLine::OffStump);
	In.Pace01 = 0.85;

	FCricketDeliveryDiagnostics D;
	FCricketDeliveryGenerator::Generate(In, A, MakeContext(2.1), &D);

	TestTrue(TEXT("Good length pitches in [4,8] m from the stumps"), D.PredictedLengthM > 4.0 && D.PredictedLengthM < 8.0);
	TestTrue(TEXT("Good length aim solve converged"), D.bAimConverged);
	return true;
}

// 4. Bouncer: pitches short AND rises above stump height by the time it reaches the striker.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingBouncerTest,
	"CricketSim.Bowling.Bouncer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingBouncerTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeExpressQuick();

	FCricketBowlingIntent Short = MakeIntent(ECricketMovement::SeamUp, ECricketLength::Bouncer, ECricketLine::Middle);
	Short.Pace01 = 0.97;
	FCricketDeliveryDiagnostics DS;
	const FCricketReleaseParameters PS = FCricketDeliveryGenerator::Generate(Short, A, MakeContext(2.1), &DS);

	FCricketBowlingIntent Good = MakeIntent(ECricketMovement::SeamUp, ECricketLength::GoodLength, ECricketLine::Middle);
	Good.Pace01 = 0.97;
	const FCricketReleaseParameters PG = FCricketDeliveryGenerator::Generate(Good, A, MakeContext(2.1));

	const double Zb = ZAtX(PredictFull(PS, HardPatch(), 2.0), StrikerX);
	const double Zg = ZAtX(PredictFull(PG, HardPatch(), 2.0), StrikerX);

	TestTrue(TEXT("Bouncer pitches short (> 8.5 m from the stumps)"), DS.PredictedLengthM > 8.5);
	TestTrue(TEXT("Bouncer reaches the striker above stump height"), Zb > CricketField::StumpHeightM);
	TestTrue(TEXT("Bouncer is higher at the striker than a good-length ball"), Zb > Zg + 0.2);
	return true;
}

// 5. Outswinger: deviates toward the off side (+Y) in conventional regime.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingOutswingTest,
	"CricketSim.Bowling.Outswing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingOutswingTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeSwingBowler();
	FCricketBowlingIntent In = MakeIntent(ECricketMovement::Outswing, ECricketLength::GoodLength, ECricketLine::OffStump);
	In.Pace01 = 0.8;
	In.SwingAmount = 0.85;

	FCricketDeliveryDiagnostics D;
	FCricketDeliveryGenerator::Generate(In, A, MakeContext(2.05), &D);

	TestTrue(TEXT("Outswinger deviates toward the off side (+Y)"), D.FreeFlightSwingM > 0.03);
	TestTrue(TEXT("Outswinger is in the conventional regime"), D.Regime == ECricketSwingRegime::Conventional);
	return true;
}

// 6. Inswinger: deviates into the right-hander (-Y).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingInswingTest,
	"CricketSim.Bowling.Inswing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingInswingTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeSwingBowler();
	FCricketBowlingIntent In = MakeIntent(ECricketMovement::Inswing, ECricketLength::Full, ECricketLine::Middle);
	In.Pace01 = 0.8;
	In.SwingAmount = 0.85;

	FCricketDeliveryDiagnostics D;
	FCricketDeliveryGenerator::Generate(In, A, MakeContext(2.05), &D);

	TestTrue(TEXT("Inswinger deviates into the right-hander (-Y)"), D.FreeFlightSwingM < -0.03);
	TestTrue(TEXT("Inswinger is in the conventional regime"), D.Regime == ECricketSwingRegime::Conventional);
	return true;
}

// 7. Reverse swing: a scuffed ball at pace, seam presented like an away swinger,
//    EMERGES tailing the OTHER way (the regime flip) — opposite the conventional case.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingReverseTest,
	"CricketSim.Bowling.ReverseSwing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingReverseTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeSwingBowler();
	const FCricketDeliveryContext Ctx = MakeContext(2.05);

	FCricketBowlingIntent Rev = MakeIntent(ECricketMovement::ReverseSwing, ECricketLength::Yorker, ECricketLine::Middle);
	Rev.Pace01 = 0.98;
	Rev.SwingAmount = 0.85;
	FCricketDeliveryDiagnostics DRev;
	FCricketDeliveryGenerator::Generate(Rev, A, Ctx, &DRev);

	// A conventional away-swinger (same +Y seam presentation) for the contrast.
	FCricketBowlingIntent Out = MakeIntent(ECricketMovement::Outswing, ECricketLength::Yorker, ECricketLine::Middle);
	Out.Pace01 = 0.98;
	Out.SwingAmount = 0.85;
	FCricketDeliveryDiagnostics DOut;
	FCricketDeliveryGenerator::Generate(Out, A, Ctx, &DOut);

	TestTrue(TEXT("Reverse delivery is classified as the reverse regime"), DRev.Regime == ECricketSwingRegime::Reverse);
	TestTrue(TEXT("Reverse swing tails into the right-hander (-Y)"), DRev.FreeFlightSwingM < -0.02);
	TestTrue(TEXT("Reverse swing is opposite the conventional away-seam"), DRev.FreeFlightSwingM * DOut.FreeFlightSwingM < 0.0);
	return true;
}

// 8. Off break: grips and turns into the right-hander (-Y) off the pitch.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingOffSpinTest,
	"CricketSim.Bowling.OffSpin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingOffSpinTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeOffSpinner();
	FCricketBowlingIntent In = MakeIntent(ECricketMovement::OffBreak, ECricketLength::GoodLength, ECricketLine::Middle);
	In.SpinAmount = 0.85;

	const FCricketReleaseParameters P = FCricketDeliveryGenerator::Generate(In, A, MakeContext(1.95));
	TestTrue(TEXT("Off break has finger-spin revs"), P.SpinRateRPM > 1200.0);
	TestTrue(TEXT("Off break turns into the right-hander (-Y) off the pitch"), BounceTurnDeltaY(P) < 0.0);
	return true;
}

// 9. Leg break: grips and turns away from the right-hander (+Y) off the pitch.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingLegSpinTest,
	"CricketSim.Bowling.LegSpin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingLegSpinTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeLegSpinner();
	FCricketBowlingIntent In = MakeIntent(ECricketMovement::LegBreak, ECricketLength::GoodLength, ECricketLine::Middle);
	In.SpinAmount = 0.85;

	const FCricketReleaseParameters P = FCricketDeliveryGenerator::Generate(In, A, MakeContext(1.95));
	TestTrue(TEXT("Leg break has wrist-spin revs"), P.SpinRateRPM > 1200.0);
	TestTrue(TEXT("Leg break turns away from the right-hander (+Y) off the pitch"), BounceTurnDeltaY(P) > 0.0);
	return true;
}

// 10. Wobble seam: low seam stability + a precession that rotates the seam in
//     flight far more than a held seam — the source of its late, inconsistent movement.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBowlingWobbleSeamTest,
	"CricketSim.Bowling.WobbleSeam", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBowlingWobbleSeamTest::RunTest(const FString&)
{
	const FCricketBowlingAction A = UCricketBowlingActionAsset::MakeSwingBowler();
	const FCricketDeliveryContext Ctx = MakeContext(2.05);

	FCricketBowlingIntent Wob = MakeIntent(ECricketMovement::WobbleSeam, ECricketLength::GoodLength, ECricketLine::OffStump);
	Wob.Pace01 = 0.85;
	const FCricketReleaseParameters PW = FCricketDeliveryGenerator::Generate(Wob, A, Ctx);

	TestTrue(TEXT("Wobble seam is released scrambled (low stability)"), PW.SeamStability < 0.3);
	TestTrue(TEXT("Wobble seam enables precession in the coefficients"),
		PW.Coefficients.WobbleSeamRateRadS > 0.0 && PW.Coefficients.WobbleSeamAmplitudeRad > 0.0);

	auto SeamDriftOverFlight = [&](const FCricketReleaseParameters& P)
	{
		FCricketBallState S = StateFromParams(P);
		const FVector Seam0 = S.SeamNormal;
		FCricketBallIntegrator I(P.BallCondition, FCricketEnvironment(), P.Coefficients);
		for (int32 i = 0; i < 300; ++i) { I.Advance(S, 0.001); } // ~0.3 s of flight
		return (S.SeamNormal - Seam0).Size();
	};

	FCricketBowlingIntent Held = MakeIntent(ECricketMovement::SeamUp, ECricketLength::GoodLength, ECricketLine::OffStump);
	Held.Pace01 = 0.85;
	const FCricketReleaseParameters PH = FCricketDeliveryGenerator::Generate(Held, A, Ctx);

	TestTrue(TEXT("Wobble seam precesses far more than a held seam"),
		SeamDriftOverFlight(PW) > SeamDriftOverFlight(PH) + 0.02);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
