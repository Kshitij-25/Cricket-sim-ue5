// Headless automation tests for the camera + replay system: framing, transitions,
// recording/playback (incl. slow motion & stepping), and physics-visualization
// accuracy. All pure — the same logic the gameplay director/replay components run.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Camera; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketCameraModel.h"
#include "CricketCameraTypes.h"
#include "CricketReplayTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCricketCameraSubjects PitchSubjects()
	{
		FCricketCameraSubjects S;
		S.BatterStumpsCm = FVector(0, 0, 0);
		S.BowlerStumpsCm = FVector(2000, 0, 0); // pitch runs along +X
		S.BallCm = FVector(1000, 0, 100);
		return S;
	}

	FCricketReplayFrame Frame(double T, const FVector& BallM)
	{
		FCricketReplayFrame F; F.Time = T; F.Ball.PositionM = BallM; F.Ball.bInFlight = true; return F;
	}
}

// 1. CAMERA MODES: each mode frames the right thing (behind batter looking down the
//    pitch; behind bowler; side-on broadcast).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketCamModesTest,
	"CricketSim.Camera.Modes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketCamModesTest::RunTest(const FString&)
{
	const FCricketCameraSubjects S = PitchSubjects();
	FCricketCameraConfig C;
	using M = FCricketCameraModel;

	const FCricketCameraPose Bat = M::ComputePose(ECricketCameraMode::Batting, S, C);
	TestTrue(TEXT("Batting cam is behind the batter (-X of the stumps)"), Bat.LocationCm.X < 0.0);
	TestTrue(TEXT("Batting cam looks down the pitch (+X)"), Bat.Rotation.Vector().X > 0.3);

	const FCricketCameraPose Bowl = M::ComputePose(ECricketCameraMode::Bowling, S, C);
	TestTrue(TEXT("Bowling cam is behind the bowler (beyond +X end)"), Bowl.LocationCm.X > S.BowlerStumpsCm.X);
	TestTrue(TEXT("Bowling cam looks back at the batter (-X)"), Bowl.Rotation.Vector().X < -0.3);

	const FCricketCameraPose Spec = M::ComputePose(ECricketCameraMode::Spectator, S, C);
	TestTrue(TEXT("Spectator cam is square of the pitch (large |Y|)"), FMath::Abs(Spec.LocationCm.Y) > 2000.0);

	const FCricketCameraPose Insp = M::ComputePose(ECricketCameraMode::PhysicsInspection, S, C);
	TestTrue(TEXT("Inspection cam uses a tight FOV"), Insp.FOVDeg < C.FOVDeg);
	return true;
}

// 2. CAMERA TRANSITIONS: a blend starts at From, ends at To, and is between midway.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketCamTransitionTest,
	"CricketSim.Camera.Transitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketCamTransitionTest::RunTest(const FString&)
{
	FCricketCameraPose A; A.LocationCm = FVector(0, 0, 0); A.FOVDeg = 60;
	FCricketCameraPose B; B.LocationCm = FVector(100, 0, 0); B.FOVDeg = 80;

	const FCricketCameraPose At0 = FCricketCameraModel::Blend(A, B, 0.0);
	const FCricketCameraPose At1 = FCricketCameraModel::Blend(A, B, 1.0);
	const FCricketCameraPose Mid = FCricketCameraModel::Blend(A, B, 0.5);

	TestTrue(TEXT("Alpha 0 -> From"), At0.LocationCm.Equals(A.LocationCm) && FMath::IsNearlyEqual(At0.FOVDeg, 60.0));
	TestTrue(TEXT("Alpha 1 -> To"), At1.LocationCm.Equals(B.LocationCm) && FMath::IsNearlyEqual(At1.FOVDeg, 80.0));
	TestTrue(TEXT("Midpoint is between"), Mid.LocationCm.X > 5.0 && Mid.LocationCm.X < 95.0);
	TestTrue(TEXT("Midpoint FOV between"), Mid.FOVDeg > 60.0 && Mid.FOVDeg < 80.0);
	return true;
}

// 3. REPLAY RECORDING: the clip is a capped ring + sparse events.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketReplayRecordTest,
	"CricketSim.Camera.ReplayRecording", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketReplayRecordTest::RunTest(const FString&)
{
	FCricketReplayClip Clip; Clip.MaxFrames = 5;
	for (int32 i = 0; i < 8; ++i) { Clip.AddFrame(Frame(i * 0.1, FVector(i, 0, 0))); }

	TestEqual(TEXT("Ring capped to MaxFrames"), Clip.NumFrames(), 5);
	TestTrue(TEXT("Oldest evicted (starts at the 4th frame)"), FMath::IsNearlyEqual(Clip.Frames[0].Time, 0.3));

	FCricketReplayEvent Bounce; Bounce.Type = ECricketReplayEventType::Bounce; Bounce.Time = 0.5; Bounce.LocationM = FVector(8, 0, 0);
	Clip.AddEvent(Bounce);
	TArray<FVector> Bounces; Clip.GetEventLocations(ECricketReplayEventType::Bounce, Bounces);
	TestEqual(TEXT("One bounce recorded"), Bounces.Num(), 1);

	TArray<FVector> Path; Clip.GetBallPath(Path);
	TestEqual(TEXT("Path has a point per frame"), Path.Num(), 5);
	return true;
}

// 4. REPLAY PLAYBACK: interpolated sampling between frames.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketReplayPlaybackTest,
	"CricketSim.Camera.ReplayPlayback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketReplayPlaybackTest::RunTest(const FString&)
{
	FCricketReplayClip Clip;
	Clip.AddFrame(Frame(0.0, FVector(0, 0, 0)));
	Clip.AddFrame(Frame(1.0, FVector(2, 4, 0)));

	const FCricketReplayFrame Mid = Clip.SampleAtTime(0.5);
	TestTrue(TEXT("Interpolated to the midpoint"), Mid.Ball.PositionM.Equals(FVector(1, 2, 0), 1e-6));

	TestEqual(TEXT("Clamp before start"), Clip.SampleAtTime(-1.0).Ball.PositionM, FVector(0, 0, 0));
	TestEqual(TEXT("Clamp after end"), Clip.SampleAtTime(5.0).Ball.PositionM, FVector(2, 4, 0));
	TestEqual(TEXT("Frame index lookup"), Clip.FrameIndexAtTime(0.4), 0);
	return true;
}

// 5. SLOW MOTION / PAUSE / STEP: the playback cursor obeys rate, pause and steps.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketReplaySlowMoTest,
	"CricketSim.Camera.SlowMotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketReplaySlowMoTest::RunTest(const FString&)
{
	FCricketReplayClip Clip;
	for (int32 i = 0; i <= 10; ++i) { Clip.AddFrame(Frame(i, FVector(i, 0, 0))); } // t = 0..10

	FCricketReplayPlayer P;
	P.Start(Clip);
	TestTrue(TEXT("Starts at the beginning"), FMath::IsNearlyEqual(P.CursorTime, 0.0));

	P.Advance(1.0); // rate 1
	TestTrue(TEXT("Real-time advance"), FMath::IsNearlyEqual(P.CursorTime, 1.0));

	P.SetRate(0.5); // slow motion
	P.Advance(1.0);
	TestTrue(TEXT("Slow-mo advances at half"), FMath::IsNearlyEqual(P.CursorTime, 1.5));

	P.Pause();
	P.Advance(2.0);
	TestTrue(TEXT("Pause holds the cursor"), FMath::IsNearlyEqual(P.CursorTime, 1.5));

	P.StepFrames(Clip, +1); // frame step (from frame index 1, t=1 -> index 2, t=2)
	TestTrue(TEXT("Frame step moves one frame"), FMath::IsNearlyEqual(P.CursorTime, 2.0));

	P.SeekNormalized(1.0);
	TestTrue(TEXT("Seek to end"), FMath::IsNearlyEqual(P.CursorTime, 10.0));
	return true;
}

// 6. PHYSICS VISUALIZATION ACCURACY: measured swing/spin deviation matches the
//    actual lateral movement in the recorded path (it visualizes, not asserts).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketReplayVizTest,
	"CricketSim.Camera.PhysicsVisualization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketReplayVizTest::RunTest(const FString&)
{
	using M = FCricketCameraModel;

	// SWING: an in-flight arc bulging ~0.3 m laterally off the release->bounce line.
	TArray<FVector> SwingPath = {
		FVector(0, 0.0, 0), FVector(1, 0.15, 0), FVector(2, 0.30, 0), FVector(3, 0.15, 0), FVector(4, 0.0, 0)
	};
	const double Swing = M::SwingDeviationM(SwingPath, /*BounceIdx*/ 4);
	TestTrue(TEXT("Swing deviation ~0.3 m"), FMath::Abs(Swing - 0.30) < 0.02);

	// SPIN: a straight approach (+X), bounce, then a ~0.4 m turn off the seam.
	TArray<FVector> SpinPath = {
		FVector(0, 0.3, 0), FVector(1, 0.3, 0), FVector(2, 0.3, 0), // bounce at index 2, moving +X
		FVector(3, 0.1, 0), FVector(4, -0.1, 0)                     // post-bounce turn
	};
	const double Spin = M::SpinDeviationM(SpinPath, /*BounceIdx*/ 2);
	TestTrue(TEXT("Spin/seam deviation ~0.4 m"), FMath::Abs(Spin - 0.40) < 0.02);

	// A straight ball deviates by ~0.
	TArray<FVector> Straight = { FVector(0,0,0), FVector(1,0,0), FVector(2,0,0), FVector(3,0,0) };
	TestTrue(TEXT("Straight ball shows ~no swing"), M::MaxLateralDeviationM(Straight, 0, 3) < 1e-6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
