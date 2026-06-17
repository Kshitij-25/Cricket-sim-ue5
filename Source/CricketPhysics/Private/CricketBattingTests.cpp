// Headless automation tests for the BATTING MOTION system: the timed swing meeting
// a live ball, contact detected geometrically, then resolved by the existing
// FCricketBatCollision. Proves timing & footwork are earned, not scripted.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Batting; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketSwingModel.h"
#include "CricketBatCollision.h"
#include "CricketBattingTypes.h"
#include "CricketBatTypes.h"
#include "CricketPhysicsTypes.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	const FVector kStance(18.0, 0.0, 0.0); // striker's guard, 18 m down the pitch

	struct FSimResult
	{
		bool bHit = false;
		FCricketContactSolution Sol;
		FCricketBatImpactReport Rep;
		FCricketBallState Out;
	};

	// Step a constant-velocity ball toward the striker while the swing clock runs,
	// exactly as the gameplay component does each tick: detect contact, then hand
	// it to the real collision solver. SwingStart is the swing-clock value at the
	// instant the ball is at Ball.Position.
	FSimResult RunSwing(
		const FCricketSwingProfile& P, const FCricketBattingInput& In,
		const FCricketBatProfile& BatP, const FCricketBallState& Ball,
		double SwingStart, double Dt = 0.004)
	{
		FSimResult R;
		FVector Pos = Ball.Position;
		double Swing = SwingStart;
		for (int32 i = 0; i < 600; ++i)
		{
			const FVector Next = Pos + Ball.Velocity * Dt;
			FCricketContactSolution S;
			if (FCricketSwingModel::DetectContact(P, In, kStance, BatP, Pos, Next, Swing, Dt, 8, S))
			{
				R.bHit = true;
				R.Sol = S;
				FCricketBallState AtContact = Ball;
				AtContact.Position = S.ContactPointM;
				FCricketBatCollision::Resolve(AtContact, S.BatAtContact, BatP, S.ContactPointM, R.Out, R.Rep);
				return R;
			}
			Pos = Next;
			Swing += Dt;
		}
		return R;
	}

	// A ball aimed to pass through the stroke's contact zone, with the swing
	// triggered so contact lands TimingShift seconds off perfect (+ = early).
	FSimResult PlayAt(
		const FCricketSwingProfile& P, const FCricketBattingInput& In,
		const FCricketBatProfile& BatP, const FVector& BallVel,
		double TimingShift, double Lead = 0.18)
	{
		const FVector Contact = kStance + P.ContactOffsetM;
		FCricketBallState Ball;
		Ball.Position = Contact - BallVel * Lead; // Lead seconds before reaching the zone
		Ball.Velocity = BallVel;
		const double SwingStart = P.DownswingTimeSec - Lead + TimingShift;
		return RunSwing(P, In, BatP, Ball, SwingStart);
	}

	FCricketBattingInput MakeInput(ECricketShotType Shot, ECricketFootwork Foot)
	{
		FCricketBattingInput In;
		In.ShotType = Shot;
		In.Footwork = Foot;
		In.bRightHanded = true;
		In.PowerScale = 1.0;
		return In;
	}
}

// 1. PERFECT TIMING: the downswing brings the sweet spot to the ball — middled,
//    fast, driven back down the ground. Nothing is scripted; it is geometry.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingPerfectTest,
	"CricketSim.Batting.PerfectTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingPerfectTest::RunTest(const FString&)
{
	const FCricketBattingInput In = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile P = FCricketSwingModel::BuildProfile(In.ShotType, In.Footwork, In.bRightHanded);
	FCricketBatProfile BatP;
	const FSimResult R = PlayAt(P, In, BatP, FVector(33, 0, -3), 0.0);

	TestTrue(TEXT("Contact made"), R.bHit && R.Rep.bMadeContact);
	TestEqual(TEXT("Timing is Perfect"), R.Sol.Timing.Quality, ECricketTimingQuality::Perfect);
	TestEqual(TEXT("Middled"), R.Rep.Region, ECricketContactRegion::Middle);
	TestTrue(TEXT("High quality contact"), R.Rep.Quality > 0.9);
	TestTrue(TEXT("Exits fast"), R.Rep.ExitSpeedMS > 28.0);
	TestTrue(TEXT("Driven back down the ground (-X)"), R.Out.Velocity.X < 0.0);
	return true;
}

// 2. EARLY TIMING: swing started ahead of the ball — sweet spot has moved into the
//    follow-through, ball meets the blade off-centre, exit suffers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingEarlyTest,
	"CricketSim.Batting.EarlyTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingEarlyTest::RunTest(const FString&)
{
	const FCricketBattingInput In = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile P = FCricketSwingModel::BuildProfile(In.ShotType, In.Footwork, In.bRightHanded);
	FCricketBatProfile BatP;
	const FSimResult Perfect = PlayAt(P, In, BatP, FVector(33, 0, -3), 0.0);
	const FSimResult Early   = PlayAt(P, In, BatP, FVector(33, 0, -3), +0.055);

	TestTrue(TEXT("Contact made"), Early.bHit);
	TestEqual(TEXT("Classified Early"), Early.Sol.Timing.Quality, ECricketTimingQuality::Early);
	TestTrue(TEXT("Early => negative timing error (bat ahead)"), Early.Sol.Timing.TimingErrorSec < 0.0);
	TestTrue(TEXT("Not middled"), Early.Rep.Region != ECricketContactRegion::Middle);
	TestTrue(TEXT("Worse exit than a perfectly timed stroke"), Early.Rep.ExitSpeedMS < Perfect.Rep.ExitSpeedMS);
	return true;
}

// 3. LATE TIMING: swing behind the ball — met before the sweet spot arrived.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingLateTest,
	"CricketSim.Batting.LateTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingLateTest::RunTest(const FString&)
{
	const FCricketBattingInput In = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile P = FCricketSwingModel::BuildProfile(In.ShotType, In.Footwork, In.bRightHanded);
	FCricketBatProfile BatP;
	const FSimResult Perfect = PlayAt(P, In, BatP, FVector(33, 0, -3), 0.0);
	const FSimResult Late    = PlayAt(P, In, BatP, FVector(33, 0, -3), -0.045);

	TestTrue(TEXT("Contact made"), Late.bHit);
	TestEqual(TEXT("Classified Late"), Late.Sol.Timing.Quality, ECricketTimingQuality::Late);
	TestTrue(TEXT("Late => positive timing error (bat behind)"), Late.Sol.Timing.TimingErrorSec > 0.0);
	TestTrue(TEXT("Not middled"), Late.Rep.Region != ECricketContactRegion::Middle);
	TestTrue(TEXT("Worse exit than a perfectly timed stroke"), Late.Rep.ExitSpeedMS < Perfect.Rep.ExitSpeedMS);
	return true;
}

// 4. FRONT-FOOT DRIVE vs a FULL ball: pressing forward middles it. The SAME full
//    ball, played back-foot, finds the sweet spot displaced — the wrong footwork
//    is punished by geometry, not by a rule.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingFrontFootTest,
	"CricketSim.Batting.FrontFootDrive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingFrontFootTest::RunTest(const FString&)
{
	FCricketBatProfile BatP;
	const FVector FullBall(33, 0, -3);

	const FCricketBattingInput FrontIn = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile FrontP = FCricketSwingModel::BuildProfile(FrontIn.ShotType, FrontIn.Footwork, true);
	const FSimResult Front = PlayAt(FrontP, FrontIn, BatP, FullBall, 0.0);

	// Same incoming ball (aimed at the front-foot contact zone), but stand back.
	const FCricketBattingInput BackIn = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::BackFoot);
	const FCricketSwingProfile BackP = FCricketSwingModel::BuildProfile(BackIn.ShotType, BackIn.Footwork, true);
	const FVector Contact = kStance + FrontP.ContactOffsetM;
	FCricketBallState Ball;
	Ball.Position = Contact - FullBall * 0.18;
	Ball.Velocity = FullBall;
	const FSimResult Back = RunSwing(BackP, BackIn, BatP, Ball, BackP.DownswingTimeSec - 0.18);

	TestTrue(TEXT("Front foot middles the full ball"), Front.bHit && Front.Rep.Region == ECricketContactRegion::Middle);
	TestTrue(TEXT("Front foot drives it (-X, fast)"), Front.Out.Velocity.X < 0.0 && Front.Rep.ExitSpeedMS > 28.0);
	TestTrue(TEXT("Back foot to a full ball is worse"),
		!Back.bHit || Back.Rep.Quality < Front.Rep.Quality);
	return true;
}

// 5. BACK-FOOT SHOT vs a SHORT ball: rocking back to a short ball pulls it away
//    cleanly to the leg side; the same short ball on the front foot is worse.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingBackFootTest,
	"CricketSim.Batting.BackFootShot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingBackFootTest::RunTest(const FString&)
{
	FCricketBatProfile BatP;
	const FVector ShortBall(29, 0, 1); // climbing toward the chest

	const FCricketBattingInput BackIn = MakeInput(ECricketShotType::PullShot, ECricketFootwork::BackFoot);
	const FCricketSwingProfile BackP = FCricketSwingModel::BuildProfile(BackIn.ShotType, BackIn.Footwork, true);
	const FSimResult Back = PlayAt(BackP, BackIn, BatP, ShortBall, 0.0);

	const FCricketBattingInput FrontIn = MakeInput(ECricketShotType::PullShot, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile FrontP = FCricketSwingModel::BuildProfile(FrontIn.ShotType, FrontIn.Footwork, true);
	const FVector Contact = kStance + BackP.ContactOffsetM;
	FCricketBallState Ball;
	Ball.Position = Contact - ShortBall * 0.18;
	Ball.Velocity = ShortBall;
	const FSimResult Front = RunSwing(FrontP, FrontIn, BatP, Ball, FrontP.DownswingTimeSec - 0.18);

	TestTrue(TEXT("Back foot connects with the short ball"), Back.bHit && Back.Rep.bMadeContact);
	TestTrue(TEXT("Pulled to the leg side (-Y)"), Back.Out.Velocity.Y < 0.0);
	TestTrue(TEXT("Solid contact"), Back.Rep.Quality > 0.6);
	TestTrue(TEXT("Front foot to a short ball is worse"),
		!Front.bHit || Front.Rep.Quality < Back.Rep.Quality);
	return true;
}

// 6. DEFENSIVE SHOT: soft hands, dead bat — contact is made but the ball dies off
//    the face and is kept down. Low exit emerges from the low bat speed & angle.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingDefensiveTest,
	"CricketSim.Batting.DefensiveShot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingDefensiveTest::RunTest(const FString&)
{
	const FCricketBattingInput In = MakeInput(ECricketShotType::DefensiveBlock, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile P = FCricketSwingModel::BuildProfile(In.ShotType, In.Footwork, In.bRightHanded);
	FCricketBatProfile BatP;
	const FSimResult Def = PlayAt(P, In, BatP, FVector(32, 0, -3), 0.0);

	// A driven equivalent, to show the defensive really is the soft one.
	const FCricketBattingInput DriveIn = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile DriveP = FCricketSwingModel::BuildProfile(DriveIn.ShotType, DriveIn.Footwork, true);
	const FSimResult Drive = PlayAt(DriveP, DriveIn, BatP, FVector(32, 0, -3), 0.0);

	TestTrue(TEXT("Defensive contact made"), Def.bHit && Def.Rep.bMadeContact);
	TestTrue(TEXT("Defensive exit is soft"), Def.Rep.ExitSpeedMS < 18.0);
	TestTrue(TEXT("Much softer than a drive"), Def.Rep.ExitSpeedMS < Drive.Rep.ExitSpeedMS);
	TestTrue(TEXT("Kept down (not lofted)"), Def.Out.Velocity.Z < 2.0);
	return true;
}

// 7. DETERMINISM: the same swing against the same ball is bit-identical (no RNG).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingDeterminismTest,
	"CricketSim.Batting.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingDeterminismTest::RunTest(const FString&)
{
	const FCricketBattingInput In = MakeInput(ECricketShotType::CoverDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile P = FCricketSwingModel::BuildProfile(In.ShotType, In.Footwork, In.bRightHanded);
	FCricketBatProfile BatP;
	const FSimResult A = PlayAt(P, In, BatP, FVector(32, 0, -3), 0.0);
	const FSimResult B = PlayAt(P, In, BatP, FVector(32, 0, -3), 0.0);

	TestTrue(TEXT("Both made contact"), A.bHit && B.bHit);
	TestTrue(TEXT("Identical exit velocity"), A.Out.Velocity == B.Out.Velocity);
	TestTrue(TEXT("Identical contact point"), A.Sol.ContactPointM == B.Sol.ContactPointM);
	TestTrue(TEXT("Cover drive goes to the off side (+Y) and forward"),
		A.Out.Velocity.Y > 0.0 && A.Out.Velocity.X < 0.0);
	return true;
}

// 8. EMERGENT TIMING ORDERING: sweeping the trigger from early to late drives the
//    timing error monotonically from negative through zero to positive.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketBattingTimingSweepTest,
	"CricketSim.Batting.TimingSweep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketBattingTimingSweepTest::RunTest(const FString&)
{
	const FCricketBattingInput In = MakeInput(ECricketShotType::StraightDrive, ECricketFootwork::FrontFoot);
	const FCricketSwingProfile P = FCricketSwingModel::BuildProfile(In.ShotType, In.Footwork, In.bRightHanded);
	FCricketBatProfile BatP;

	// Sweeping the trigger early -> late should drive the timing error up through
	// zero. (TimingErrorSec > 0 = late; so it must INCREASE as we go more late.)
	double Prev = TNumericLimits<double>::Lowest();
	bool bMonotonic = true;
	for (double Shift : {0.05, 0.025, 0.0, -0.025, -0.05}) // early -> late
	{
		const FSimResult R = PlayAt(P, In, BatP, FVector(33, 0, -3), Shift);
		if (!R.bHit) { bMonotonic = false; break; }
		if (R.Sol.Timing.TimingErrorSec < Prev - 1e-6) { bMonotonic = false; }
		Prev = R.Sol.Timing.TimingErrorSec;
	}
	TestTrue(TEXT("Timing error increases monotonically from early to late"), bMonotonic);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
