// Headless automation tests for the stadium as a simulation environment: boundary
// geometry, four/six classification from a real ball path, boundary catches,
// ground-size sensitivity, and field-placement validation. All pure.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Stadium; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketStadiumModel.h"
#include "CricketStadiumTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCricketGroundDimensions Ground(double Straight = 75.0, double Square = 68.0)
	{
		FCricketGroundDimensions D;
		D.CenterM = FVector::ZeroVector;
		D.PitchAxis = FVector(1, 0, 0);
		D.StraightBoundaryM = Straight;
		D.SquareBoundaryM = Square;
		D.RopeHeightM = 0.15;
		D.PitchLengthM = 20.12;
		return D;
	}

	using M = FCricketStadiumModel;
}

// 1. BOUNDARY GEOMETRY: the ellipse radius is the straight/square distance on-axis,
//    and inside/outside is correct.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketStadiumGeometryTest,
	"CricketSim.Stadium.BoundaryGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketStadiumGeometryTest::RunTest(const FString&)
{
	const FCricketGroundDimensions D = Ground(75, 68);

	TestTrue(TEXT("Straight radius (on-axis)"), FMath::IsNearlyEqual(M::BoundaryRadiusAtAngleM(D, 0.0), 75.0, 1e-6));
	TestTrue(TEXT("Square radius (perpendicular)"), FMath::IsNearlyEqual(M::BoundaryRadiusAtAngleM(D, PI / 2), 68.0, 1e-6));

	TestTrue(TEXT("Centre is inside"), M::IsInsideBoundary(D, FVector(0, 0, 0)));
	TestTrue(TEXT("Just inside straight"), M::IsInsideBoundary(D, FVector(74, 0, 0)));
	TestFalse(TEXT("Beyond straight is outside"), M::IsInsideBoundary(D, FVector(76, 0, 0)));
	TestTrue(TEXT("Just inside square"), M::IsInsideBoundary(D, FVector(0, 67, 0)));
	TestFalse(TEXT("Beyond square is outside"), M::IsInsideBoundary(D, FVector(0, 69, 0)));
	return true;
}

// 2. FOUR: a ball that bounces inside the rope and then crosses it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketStadiumFourTest,
	"CricketSim.Stadium.Four", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketStadiumFourTest::RunTest(const FString&)
{
	const FCricketGroundDimensions D = Ground();
	// Low along the ground: bounces at index 2 (X=20), crosses the straight rope at X=80.
	TArray<FVector> Path = {
		FVector(0, 0, 0.5), FVector(10, 0, 0.3), FVector(20, 0, 0.05),
		FVector(40, 0, 0.4), FVector(60, 0, 0.3), FVector(80, 0, 0.2)
	};
	FVector Cross;
	const ECricketBoundaryResult R = M::ClassifyBoundary(D, Path, /*FirstBounceIdx*/ 2, Cross);
	TestEqual(TEXT("Bounced inside then over = four"), R, ECricketBoundaryResult::Four);
	TestTrue(TEXT("Crossing recorded near the rope"), Cross.X > 75.0);
	return true;
}

// 3. SIX: a ball that clears the rope on the full (no bounce inside).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketStadiumSixTest,
	"CricketSim.Stadium.Six", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketStadiumSixTest::RunTest(const FString&)
{
	const FCricketGroundDimensions D = Ground();
	TArray<FVector> Path = {
		FVector(0, 0, 1), FVector(20, 0, 10), FVector(40, 0, 16), FVector(60, 0, 14), FVector(80, 0, 6)
	};
	FVector Cross;
	const ECricketBoundaryResult R = M::ClassifyBoundary(D, Path, /*FirstBounceIdx*/ INDEX_NONE, Cross);
	TestEqual(TEXT("Cleared the rope = six"), R, ECricketBoundaryResult::Six);
	TestTrue(TEXT("Crossing is airborne"), Cross.Z > D.RopeHeightM);
	return true;
}

// 4. BOUNDARY CATCHES: a catch inside the rope is out; over the rope it's a six.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketStadiumCatchTest,
	"CricketSim.Stadium.BoundaryCatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketStadiumCatchTest::RunTest(const FString&)
{
	const FCricketGroundDimensions D = Ground(75, 68);

	TestEqual(TEXT("Caught inside the rope = out"),
		M::ValidateBoundaryCatch(D, FVector(70, 0, 1.5)), ECricketBoundaryResult::CaughtAtBoundary);
	TestEqual(TEXT("Foot over the rope = six"),
		M::ValidateBoundaryCatch(D, FVector(78, 0, 1.5)), ECricketBoundaryResult::Six);

	// The SAME catch point is a six on a smaller ground (the rope is closer in).
	const FCricketGroundDimensions Small = Ground(65, 60);
	TestEqual(TEXT("Same spot is over the rope on a small ground"),
		M::ValidateBoundaryCatch(Small, FVector(70, 0, 1.5)), ECricketBoundaryResult::Six);
	return true;
}

// 5. DIFFERENT GROUND SIZES: the same hit is a six on a small ground, in-play on a
//    big one — the boundary system is ground-relative, not absolute.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketStadiumGroundSizeTest,
	"CricketSim.Stadium.GroundSizes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketStadiumGroundSizeTest::RunTest(const FString&)
{
	TArray<FVector> Path = {
		FVector(0, 0, 1), FVector(20, 0, 9), FVector(45, 0, 12), FVector(65, 0, 8), FVector(80, 0, 3)
	};
	FVector Cross;

	const ECricketBoundaryResult Small = M::ClassifyBoundary(Ground(65, 60), Path, INDEX_NONE, Cross);
	TestEqual(TEXT("Six on a small ground"), Small, ECricketBoundaryResult::Six);

	const ECricketBoundaryResult Big = M::ClassifyBoundary(Ground(90, 85), Path, INDEX_NONE, Cross);
	TestEqual(TEXT("In play on a big ground"), Big, ECricketBoundaryResult::InPlay);
	return true;
}

// 6. FIELD PLACEMENT VALIDATION: positions are inside the rope, on the correct side,
//    behind/forward as expected, and mirror for a left-hander; depth scales with size.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketStadiumFieldTest,
	"CricketSim.Stadium.FieldPlacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketStadiumFieldTest::RunTest(const FString&)
{
	const FCricketGroundDimensions D = Ground();
	const FVector Striker = D.StrikerStumpsM();

	// Every position in the default field is inside the boundary.
	for (ECricketFieldPosition P : M::DefaultField().Positions)
	{
		TestTrue(TEXT("Position is inside the rope"), M::IsInsideBoundary(D, M::FieldPositionWorldM(D, P, true)));
	}

	// Off-side positions are +Y (RH); leg-side are -Y.
	TestTrue(TEXT("Cover is off side (+Y)"), M::FieldPositionWorldM(D, ECricketFieldPosition::Cover, true).Y > Striker.Y);
	TestTrue(TEXT("Point is off side (+Y)"), M::FieldPositionWorldM(D, ECricketFieldPosition::Point, true).Y > Striker.Y);
	TestTrue(TEXT("Midwicket is leg side (-Y)"), M::FieldPositionWorldM(D, ECricketFieldPosition::Midwicket, true).Y < Striker.Y);
	TestTrue(TEXT("Square leg is leg side (-Y)"), M::FieldPositionWorldM(D, ECricketFieldPosition::SquareLeg, true).Y < Striker.Y);

	// Slip stands behind the batter; mid-off is in front (toward the bowler).
	TestTrue(TEXT("Slip is behind the striker"), M::FieldPositionWorldM(D, ECricketFieldPosition::Slip, true).X < Striker.X);
	TestTrue(TEXT("Mid-off is in front"), M::FieldPositionWorldM(D, ECricketFieldPosition::MidOff, true).X > Striker.X);

	// Left-hander mirrors the off side to -Y.
	TestTrue(TEXT("Cover mirrors to -Y for a left-hander"), M::FieldPositionWorldM(D, ECricketFieldPosition::Cover, false).Y < Striker.Y);

	// Depth scales with ground size: long-on is further out on a bigger ground.
	const double Small = (M::FieldPositionWorldM(Ground(65, 60), ECricketFieldPosition::LongOn, true) - Striker).Size();
	const double Large = (M::FieldPositionWorldM(Ground(90, 85), ECricketFieldPosition::LongOn, true) - Striker).Size();
	TestTrue(TEXT("Deep positions scale with ground size"), Large > Small + 10.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
