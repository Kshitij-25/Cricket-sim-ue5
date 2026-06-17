// Headless automation tests for the deterministic ball-physics core.
// These guard the physical guarantees the rest of the game relies on: identical
// inputs -> identical flight, and correct force signs for swing/Magnus.
// Run via the editor Automation panel or:
//   UnrealEditor-Cmd CricketSim.uproject -ExecCmds="Automation RunTests CricketSim.Physics" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketAerodynamics.h"
#include "CricketBallIntegrator.h"
#include "CricketPitchInteraction.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	// A seam tilted ~20 deg to a +X flight line, normal biased to +Y (off side).
	FVector OutswingSeamNormal()
	{
		const double S = FMath::Sin(0.349); // sin(20 deg)
		const double C = FMath::Cos(0.349);
		return FVector(S, C, 0.0).GetSafeNormal();
	}
}

// 1. Determinism & frame-rate consistency.
//   (a) The TRUE guarantee: an identical delta-stream reproduces bit-for-bit.
//   (b) The fixed sub-step makes flight frame-rate INDEPENDENT to within at most
//       one sub-step of carried remainder (it is not bit-identical across
//       different slicings because the leftover partial step differs).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPhysicsDeterminismTest,
	"CricketSim.Physics.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPhysicsDeterminismTest::RunTest(const FString&)
{
	auto MakeState = []()
	{
		FCricketBallState S;
		S.Position = FVector(0, 0, 2.0);
		S.Velocity = FVector(35, 0, -2);
		S.AngularVelocity = FVector(0, -120, 0); // backspin
		S.SeamNormal = OutswingSeamNormal();
		return S;
	};

	// (a) Bit-identical reproduction from the same delta-stream.
	{
		FCricketBallState A = MakeState();
		FCricketBallState B = MakeState();
		FCricketBallIntegrator I1{FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients()};
		FCricketBallIntegrator I2 = I1;
		for (int32 i = 0; i < 30; ++i) { I1.Advance(A, 1.0 / 60.0); }
		for (int32 i = 0; i < 30; ++i) { I2.Advance(B, 1.0 / 60.0); }
		TestTrue(TEXT("Determinism: identical delta-stream reproduces position bit-for-bit"),
			A.Position == B.Position);
		TestTrue(TEXT("Determinism: identical delta-stream reproduces velocity bit-for-bit"),
			A.Velocity == B.Velocity);
	}

	// (b) Frame-rate independence to within one sub-step (1 ms of motion).
	{
		FCricketBallState A = MakeState();
		FCricketBallState B = MakeState();
		FCricketBallIntegrator I1{FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients()};
		FCricketBallIntegrator I2 = I1;
		for (int32 i = 0; i < 30; ++i) { I1.Advance(A, 1.0 / 60.0); }   // ~0.5 s @ 60 fps
		for (int32 i = 0; i < 50; ++i) { I2.Advance(B, 1.0 / 100.0); }  // ~0.5 s @ 100 fps
		// One 1 ms sub-step at ~35 m/s is ~0.035 m; allow a small margin.
		TestTrue(TEXT("Frame-rate consistency: positions agree to within one sub-step"),
			A.Position.Equals(B.Position, 0.05));
		TestTrue(TEXT("Frame-rate consistency: velocities agree to within one sub-step"),
			A.Velocity.Equals(B.Velocity, 0.05));
	}
	return true;
}

// 2. Magnus sign: backspin (omega = -Y for +X flight) must produce upward lift.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMagnusBackspinTest,
	"CricketSim.Physics.MagnusBackspin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMagnusBackspinTest::RunTest(const FString&)
{
	FCricketBallState S;
	S.Velocity = FVector(40, 0, 0);
	S.AngularVelocity = FVector(0, -150, 0); // backspin
	S.SeamNormal = FVector(1, 0, 0);          // seam edge-on => no swing contamination

	const FCricketAeroResult R = FCricketAerodynamics::Evaluate(
		S, FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients());

	TestTrue(TEXT("Backspin generates upward Magnus force (carry)"), R.Force.Z > 0.0);
	return true;
}

// 3. Conventional swing direction: seam biased +Y, polished, sub-critical speed
//    => side force toward +Y. Mirror the seam => force flips to -Y.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketConventionalSwingTest,
	"CricketSim.Physics.ConventionalSwing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketConventionalSwingTest::RunTest(const FString&)
{
	FCricketBallSurface Surface; // shine=1, roughness=0 by default
	FCricketAeroCoefficients Coeffs;
	FCricketEnvironment Env;

	FCricketBallState S;
	S.Velocity = FVector(25, 0, 0);            // below 30 m/s transition => conventional
	S.AngularVelocity = FVector(20, 0, 0);     // gyroscopic spin to hold the seam
	S.SeamNormal = OutswingSeamNormal();       // +Y biased

	const FCricketAeroResult R = FCricketAerodynamics::Evaluate(S, Surface, Env, Coeffs);
	TestTrue(TEXT("Conventional swing pushes toward seam side (+Y)"), R.Force.Y > 0.0);

	// Mirror the seam: force must flip sign.
	S.SeamNormal = FVector(S.SeamNormal.X, -S.SeamNormal.Y, S.SeamNormal.Z);
	const FCricketAeroResult R2 = FCricketAerodynamics::Evaluate(S, Surface, Env, Coeffs);
	TestTrue(TEXT("Mirrored seam swings the other way (-Y)"), R2.Force.Y < 0.0);
	return true;
}

// 4. Reverse swing: same seam, but fast + roughened => lateral force REVERSES
//    relative to the conventional case.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketReverseSwingTest,
	"CricketSim.Physics.ReverseSwing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketReverseSwingTest::RunTest(const FString&)
{
	FCricketAeroCoefficients Coeffs;
	FCricketEnvironment Env;
	const FVector Seam = OutswingSeamNormal(); // +Y biased

	// Conventional reference: new ball, slow.
	FCricketBallSurface NewBall; // shine 1, rough 0
	FCricketBallState Conv;
	Conv.Velocity = FVector(25, 0, 0);
	Conv.SeamNormal = Seam;
	const double ConvY = FCricketAerodynamics::Evaluate(Conv, NewBall, Env, Coeffs).Force.Y;

	// Reverse case: rough ball, high pace.
	FCricketBallSurface OldBall; OldBall.Roughness = 1.0; OldBall.ShineAsymmetry = 0.2;
	FCricketBallState Rev;
	Rev.Velocity = FVector(40, 0, 0);
	Rev.SeamNormal = Seam;
	const double RevY = FCricketAerodynamics::Evaluate(Rev, OldBall, Env, Coeffs).Force.Y;

	TestTrue(TEXT("Conventional side force is +Y"), ConvY > 0.0);
	TestTrue(TEXT("Reverse swing flips the side force to -Y"), RevY < 0.0);
	return true;
}

// 5. Magnus drift: side-spin (axis +Z) for a +X delivery => sideways drift (+Y).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMagnusDriftTest,
	"CricketSim.Physics.MagnusDrift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMagnusDriftTest::RunTest(const FString&)
{
	FCricketBallState S;
	S.Velocity = FVector(28, 0, 0);
	S.AngularVelocity = FVector(0, 0, 180); // side-spin about vertical
	S.SeamNormal = FVector(1, 0, 0);         // edge-on => no swing contamination
	const FCricketAeroResult R = FCricketAerodynamics::Evaluate(
		S, FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients());
	TestTrue(TEXT("Side-spin produces lateral drift force"), FMath::Abs(R.Force.Y) > 0.0 && R.Force.Y > 0.0);
	return true;
}

// 6. Magnus dip: top-spin (axis +Y) for a +X delivery => extra downforce (dip).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMagnusDipTest,
	"CricketSim.Physics.MagnusDip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMagnusDipTest::RunTest(const FString&)
{
	FCricketBallState Top;
	Top.Velocity = FVector(28, 0, 0);
	Top.AngularVelocity = FVector(0, 200, 0); // top-spin
	Top.SeamNormal = FVector(1, 0, 0);
	const FCricketAeroResult RTop = FCricketAerodynamics::Evaluate(
		Top, FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients());
	TestTrue(TEXT("Top-spin adds downward (dip) Magnus force"), RTop.Force.Z < 0.0);
	return true;
}

// 7. Turn off the pitch: spin about the line of flight (+X) grips and deflects
//    the ball laterally; reversing the spin reverses the deflection.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchTurnTest,
	"CricketSim.Physics.PitchTurn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchTurnTest::RunTest(const FString&)
{
	FCricketSurfacePatch Patch; Patch.Hardness = 0.7; Patch.Friction = 0.6;
	FCricketImpact Impact;
	Impact.ContactNormal = FVector(0, 0, 1);
	Impact.SeamContact = 0.0; // isolate spin-grip turn from seam movement
	Impact.Variance = 0.0;

	auto BounceLateral = [&](double SpinX)
	{
		FCricketBallState S;
		S.Velocity = FVector(30, 0, -8);
		S.AngularVelocity = FVector(SpinX, 0, 0); // spin about the flight line
		FCricketPitchInteraction::ResolveBounce(S, Patch, Impact);
		return S.Velocity.Y;
	};

	const double YPos = BounceLateral(+250.0);
	const double YNeg = BounceLateral(-250.0);
	TestTrue(TEXT("Spin about +X turns the ball one way"), YPos < 0.0);
	TestTrue(TEXT("Reversing the spin turns it the other way"), YNeg > 0.0);
	TestTrue(TEXT("Turn is symmetric"), FMath::IsNearlyEqual(YPos, -YNeg, 1e-6));
	return true;
}

// 8. Late movement: lateral displacement from swing ACCELERATES through flight
//    (more sideways movement in the second half than the first) — "late swing".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketLateMovementTest,
	"CricketSim.Physics.LateMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketLateMovementTest::RunTest(const FString&)
{
	FCricketBallState S;
	S.Position = FVector(0, 0, 2.0);
	S.Velocity = FVector(28, 0, -1);     // mostly-conventional regime
	S.AngularVelocity = FVector(15, 0, 0); // light gyroscopic seam hold
	S.SeamNormal = OutswingSeamNormal();

	FCricketBallIntegrator I{FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients()};
	auto AdvanceTo = [&](double Seconds)
	{
		// step in 1/600 s chunks to a target time
		const int32 Steps = FMath::RoundToInt(Seconds * 600.0);
		FCricketBallState Local = S;
		FCricketBallIntegrator LocalI = I;
		for (int32 i = 0; i < Steps; ++i) { LocalI.Advance(Local, 1.0 / 600.0); }
		return Local.Position.Y;
	};

	const double Y0   = 0.0;
	const double YMid = AdvanceTo(0.25);
	const double YEnd = AdvanceTo(0.50);
	const double FirstHalf = YMid - Y0;
	const double SecondHalf = YEnd - YMid;
	TestTrue(TEXT("Swing produces lateral movement"), FMath::Abs(YEnd) > 1e-3);
	TestTrue(TEXT("Movement is LATE (second half exceeds first)"), SecondHalf > FirstHalf);
	return true;
}

// 9. Predictor consistency: the predicted path equals the actual integrated path
//    (same model, same inputs) — proving prediction is physics, not a guess.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPredictorConsistencyTest,
	"CricketSim.Physics.PredictorConsistency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPredictorConsistencyTest::RunTest(const FString&)
{
	FCricketBallState Init;
	Init.Position = FVector(0, 0, 2.0);
	Init.Velocity = FVector(33, 0, 1);
	Init.AngularVelocity = FVector(0, -100, 0);
	Init.SeamNormal = OutswingSeamNormal();

	FCricketBallIntegrator I{FCricketBallSurface(), FCricketEnvironment(), FCricketAeroCoefficients()};

	FCricketPredictionParams P;
	P.MaxTime = 0.4;
	P.SampleInterval = 0.01;
	P.PitchPlaneZ = -100.0;     // far below => no bounce in this window
	P.bResolveBounces = false;
	const FCricketTrajectoryPrediction Pred = FCricketTrajectoryPredictor::Predict(Init, I, P);

	// Actual: advance a copy with the SAME step the predictor used.
	FCricketBallState Actual = Init;
	FCricketBallIntegrator AI = I;
	const int32 N = FMath::RoundToInt(P.MaxTime / P.SampleInterval);
	for (int32 i = 0; i < N; ++i) { AI.Advance(Actual, P.SampleInterval); }

	TestTrue(TEXT("Predictor produced samples"), Pred.Samples.Num() > 0);
	const FVector PredictedEnd = Pred.Samples.Last().Position;
	TestTrue(TEXT("Predicted endpoint matches actual integration"),
		PredictedEnd.Equals(Actual.Position, 1e-6));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
