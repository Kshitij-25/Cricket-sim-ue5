// Headless automation tests for the bat-ball collision system.
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Bat; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketBatCollision.h"
#include "CricketShotGenerator.h"
#include "CricketBatTypes.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	FCricketBallState IncomingBall()
	{
		FCricketBallState B;
		B.Position = FVector(0, 0, 0.6);
		B.Velocity = FVector(33, 0, -2); // toward the striker, slightly descending
		return B;
	}

	// A straight-drive bat with the sweet spot at the given contact point.
	FCricketBatState DriveBat(const FVector& SweetSpot, double Speed = 26.0)
	{
		FCricketBatState Bat;
		Bat.SweetSpotLocation = SweetSpot;
		Bat.FaceNormal = FVector(-1, 0, 0.12);
		Bat.LongAxis = FVector(0, 0, 1);
		Bat.LinearVelocity = FVector(-1, 0, 0.2).GetSafeNormal() * Speed;
		Bat.AngularVelocity = FVector::ZeroVector;
		Bat.Orthonormalize();
		return Bat;
	}
}

// 1. Centre (sweet-spot) impact: high exit, clean (small deflection), middle.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatCenterTest,
	"CricketSim.Bat.CenterImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatCenterTest::RunTest(const FString&)
{
	const FCricketBallState In = IncomingBall();
	const FCricketBatState Bat = DriveBat(In.Position);
	FCricketBatProfile Profile;
	FCricketBallState Out; FCricketBatImpactReport R;
	const bool bHit = FCricketBatCollision::Resolve(In, Bat, Profile, In.Position, Out, R);

	TestTrue(TEXT("Contact made"), bHit && R.bMadeContact);
	TestEqual(TEXT("Region is middle"), R.Region, ECricketContactRegion::Middle);
	TestTrue(TEXT("Middled drive exits fast"), R.ExitSpeedMS > 28.0);
	TestTrue(TEXT("Clean hit deflects little"), R.DeflectionAngleDeg < 15.0);
	TestTrue(TEXT("Ball driven back down the ground (-X)"), Out.Velocity.X < 0.0);
	return true;
}

// 2. Edge impact: offset across the face -> thin/thick edge, high edge factor,
//    much lower exit, ball squirts off at a big deflection.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatEdgeTest,
	"CricketSim.Bat.EdgeImpact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatEdgeTest::RunTest(const FString&)
{
	const FCricketBallState In = IncomingBall();
	FCricketBatProfile Profile;
	const FCricketBatState Bat = DriveBat(In.Position);

	// Contact 5 cm across the face (near the edge: half-width ~5.4 cm).
	const FVector EdgeContact = In.Position + Bat.WidthAxis * 0.05;
	FCricketBallState Out; FCricketBatImpactReport R;
	FCricketBatCollision::Resolve(In, Bat, Profile, EdgeContact, Out, R);

	TestTrue(TEXT("Classified as an edge"), R.bIsEdge);
	TestTrue(TEXT("High edge factor"), R.EdgeFactor > 0.8);
	const bool bSideEdge = R.Region == ECricketContactRegion::ThinEdge || R.Region == ECricketContactRegion::ThickEdge;
	TestTrue(TEXT("Region is a side edge"), bSideEdge);
	TestTrue(TEXT("Edge kills exit speed vs incoming"), R.ExitSpeedMS < In.Velocity.Size());
	TestTrue(TEXT("Edge deflects significantly"), R.DeflectionAngleDeg > 15.0);
	return true;
}

// 3. Bat speed monotonicity: faster bat -> faster exit at the same contact.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatSpeedTest,
	"CricketSim.Bat.BatSpeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatSpeedTest::RunTest(const FString&)
{
	const FCricketBallState In = IncomingBall();
	FCricketBatProfile Profile;
	auto ExitForSpeed = [&](double Speed)
	{
		FCricketBallState Out; FCricketBatImpactReport R;
		FCricketBatCollision::Resolve(In, DriveBat(In.Position, Speed), Profile, In.Position, Out, R);
		return R.ExitSpeedMS;
	};
	const double Low = ExitForSpeed(8.0);
	const double High = ExitForSpeed(30.0);
	TestTrue(TEXT("High bat speed yields higher exit than low"), High > Low);
	return true;
}

// 4. Spin conditions: opposite incoming side-spin deflects the ball oppositely.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatSpinTest,
	"CricketSim.Bat.SpinConditions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatSpinTest::RunTest(const FString&)
{
	FCricketBatProfile Profile;
	auto ExitYForSpin = [&](double SpinZ)
	{
		FCricketBallState In = IncomingBall();
		In.AngularVelocity = FVector(0, 0, SpinZ);
		const FCricketBatState Bat = DriveBat(In.Position);
		FCricketBallState Out; FCricketBatImpactReport R;
		FCricketBatCollision::Resolve(In, Bat, Profile, In.Position, Out, R);
		return Out.Velocity.Y;
	};
	const double YPos = ExitYForSpin(+200.0);
	const double YNeg = ExitYForSpin(-200.0);
	TestTrue(TEXT("Incoming spin alters exit direction"), FMath::Abs(YPos - YNeg) > 0.1);
	TestTrue(TEXT("Opposite spin deflects oppositely"), YPos * YNeg < 0.0);
	return true;
}

// 5. Impact-location sweep: exit speed falls monotonically toward the toe.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatLocationSweepTest,
	"CricketSim.Bat.ImpactLocationSweep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatLocationSweepTest::RunTest(const FString&)
{
	const FCricketBallState In = IncomingBall();
	FCricketBatProfile Profile;
	const FCricketBatState Bat = DriveBat(In.Position);

	double Prev = TNumericLimits<double>::Max();
	bool bMonotonic = true;
	for (double d = 0.0; d <= 0.16 + 1e-9; d += 0.04)
	{
		const FVector Contact = In.Position - Bat.LongAxis * d; // toward the toe
		FCricketBallState Out; FCricketBatImpactReport R;
		FCricketBatCollision::Resolve(In, Bat, Profile, Contact, Out, R);
		if (R.ExitSpeedMS > Prev + 1e-6) { bMonotonic = false; }
		Prev = R.ExitSpeedMS;
	}
	TestTrue(TEXT("Exit speed decreases moving away from the sweet spot"), bMonotonic);
	return true;
}

// 6. Determinism: identical inputs give an identical outcome (no RNG anywhere).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatDeterminismTest,
	"CricketSim.Bat.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatDeterminismTest::RunTest(const FString&)
{
	const FCricketBallState In = IncomingBall();
	FCricketBatProfile Profile;
	const FCricketBatState Bat = DriveBat(In.Position);
	FCricketBallState O1, O2; FCricketBatImpactReport R1, R2;
	FCricketBatCollision::Resolve(In, Bat, Profile, In.Position, O1, R1);
	FCricketBatCollision::Resolve(In, Bat, Profile, In.Position, O2, R2);
	TestTrue(TEXT("Identical exit velocity"), O1.Velocity == O2.Velocity);
	TestTrue(TEXT("Identical exit spin"), O1.AngularVelocity == O2.AngularVelocity);
	return true;
}

// 7. Energy is never created: ball KE out <= total KE in.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBatEnergyTest,
	"CricketSim.Bat.EnergyConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBatEnergyTest::RunTest(const FString&)
{
	FCricketBatProfile Profile;
	bool bOk = true;
	for (double Speed : {5.0, 15.0, 25.0, 35.0})
	{
		const FCricketBallState In = IncomingBall();
		FCricketBallState Out; FCricketBatImpactReport R;
		FCricketBatCollision::Resolve(In, DriveBat(In.Position, Speed), Profile, In.Position, Out, R);
		if (R.EnergyOutBallJ > R.EnergyInJ * 1.0001) { bOk = false; }
	}
	TestTrue(TEXT("Ball KE out never exceeds total KE in"), bOk);
	return true;
}

// 8. Shot generator: each MVP shot sends the ball into the right region.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketShotHemisphereTest,
	"CricketSim.Bat.ShotHemispheres", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketShotHemisphereTest::RunTest(const FString&)
{
	FCricketBatProfile Profile;
	auto PlayShot = [&](ECricketShotType Type, FCricketBallState& Out)
	{
		FCricketShotIntent Intent; Intent.ShotType = Type; Intent.bRightHanded = true;
		FCricketBallState In = IncomingBall();
		FCricketBatState Bat; FVector Contact;
		FCricketShotGenerator::GenerateBatState(Intent, In, Profile, Bat, Contact);
		FCricketBatImpactReport R;
		return FCricketBatCollision::Resolve(In, Bat, Profile, Contact, Out, R);
	};

	FCricketBallState Straight, Cover, Pull, Block;
	PlayShot(ECricketShotType::StraightDrive, Straight);
	PlayShot(ECricketShotType::CoverDrive, Cover);
	PlayShot(ECricketShotType::PullShot, Pull);
	PlayShot(ECricketShotType::DefensiveBlock, Block);

	TestTrue(TEXT("Straight drive goes back down the ground (-X)"), Straight.Velocity.X < 0.0);
	TestTrue(TEXT("Cover drive goes to the off side (+Y) and forward"), Cover.Velocity.Y > 0.0 && Cover.Velocity.X < 0.0);
	TestTrue(TEXT("Pull goes to the leg side (-Y)"), Pull.Velocity.Y < 0.0);
	TestTrue(TEXT("Block is soft (low exit) and kept down"), Block.Velocity.Size() < Straight.Velocity.Size() && Block.Velocity.Z < 0.0);
	return true;
}

// 9. Mistiming is physics, not luck: a timing error moves the contact off the
//    sweet spot to a top edge; perfect timing stays in the middle.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketShotMistimeTest,
	"CricketSim.Bat.MistimeProducesEdge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketShotMistimeTest::RunTest(const FString&)
{
	FCricketBatProfile Profile;
	auto RegionFor = [&](double TimingErr)
	{
		FCricketShotIntent Intent; Intent.ShotType = ECricketShotType::StraightDrive;
		Intent.TimingErrorSec = TimingErr;
		FCricketBallState In = IncomingBall();
		FCricketBatState Bat; FVector Contact;
		FCricketShotGenerator::GenerateBatState(Intent, In, Profile, Bat, Contact);
		FCricketBallState Out; FCricketBatImpactReport R;
		FCricketBatCollision::Resolve(In, Bat, Profile, Contact, Out, R);
		return R.Region;
	};
	TestEqual(TEXT("Perfect timing -> middle"), RegionFor(0.0), ECricketContactRegion::Middle);
	TestEqual(TEXT("Late timing -> top edge"), RegionFor(0.015), ECricketContactRegion::TopEdge);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
