// Headless automation tests for the animation layer: the four state machines and
// the notify-timeline engine. These prove the TIMING (release/impact/catch/throw)
// and state transitions without any skeletal mesh — the same montage/notify model
// a real Anim Blueprint would run.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Anim; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketAnimationModel.h"
#include "CricketAnimationTypes.h"
#include "CricketBattingTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FFired { ECricketAnimNotify Type; double Time; };

	// Play a montage to completion, recording each notify and the time it fired.
	TArray<FFired> PlayOut(const FCricketActionMontage& M, double Dt = 0.005)
	{
		FCricketMontagePlayer P; P.Start(M);
		TArray<FFired> Out;
		int32 Guard = 0;
		while (P.bPlaying && Guard++ < 200000)
		{
			TArray<ECricketAnimNotify> Fired;
			P.Advance(Dt, Fired);
			for (ECricketAnimNotify T : Fired) { Out.Add({ T, P.Time }); }
		}
		return Out;
	}

	int32 CountOf(const TArray<FFired>& F, ECricketAnimNotify T)
	{
		int32 N = 0; for (const FFired& X : F) { if (X.Type == T) { ++N; } } return N;
	}
	double TimeOf(const TArray<FFired>& F, ECricketAnimNotify T)
	{
		for (const FFired& X : F) { if (X.Type == T) { return X.Time; } } return -1.0;
	}
}

// 1. LOCOMOTION STATE MACHINE: speed (and how it is changing) selects the gait.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimLocomotionTest,
	"CricketSim.Anim.Locomotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimLocomotionTest::RunTest(const FString&)
{
	FCricketLocomotionConfig C;
	using M = FCricketAnimationModel;
	const auto Idle = ECricketLocomotionState::Idle;

	TestEqual(TEXT("Still -> Idle"), M::ClassifyLocomotion(0.0, 0, 0, Idle, C).State, ECricketLocomotionState::Idle);
	TestEqual(TEXT("1 m/s -> Walk"), M::ClassifyLocomotion(1.0, 0, 0, Idle, C).State, ECricketLocomotionState::Walk);
	TestEqual(TEXT("3 m/s -> Jog"), M::ClassifyLocomotion(3.0, 0, 0, Idle, C).State, ECricketLocomotionState::Jog);
	TestEqual(TEXT("6 m/s -> Sprint"), M::ClassifyLocomotion(6.0, 0, 0, Idle, C).State, ECricketLocomotionState::Sprint);
	TestEqual(TEXT("Turning in place -> Turn"), M::ClassifyLocomotion(0.0, 220, 0, Idle, C).State, ECricketLocomotionState::Turn);
	TestEqual(TEXT("Hard decel -> Stop"), M::ClassifyLocomotion(2.0, 0, -12, ECricketLocomotionState::Jog, C).State, ECricketLocomotionState::Stop);

	TestTrue(TEXT("Gait blend rises with speed"),
		M::ClassifyLocomotion(6.0, 0, 0, Idle, C).GaitBlend > M::ClassifyLocomotion(1.0, 0, 0, Idle, C).GaitBlend);
	return true;
}

// 2. BOWLING RELEASE TIMING: the BallRelease notify fires once, at the action's
//    release time — this is what tells the bowling system WHEN to release.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimReleaseTest,
	"CricketSim.Anim.BowlingReleaseTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimReleaseTest::RunTest(const FString&)
{
	FCricketBowlingActionTimeline T; // defaults
	const FCricketActionMontage M = FCricketAnimationModel::MakeBowlingMontage(T);
	const double Dt = 0.005;
	const TArray<FFired> F = PlayOut(M, Dt);

	TestEqual(TEXT("Released exactly once"), CountOf(F, ECricketAnimNotify::BallRelease), 1);
	const double FireT = TimeOf(F, ECricketAnimNotify::BallRelease);
	TestTrue(TEXT("Release at the scheduled time"),
		FireT >= T.ReleaseTimeSec() - 1e-9 && FireT <= T.ReleaseTimeSec() + Dt + 1e-9);

	// Before the release the bowler is running up / striding; not yet released.
	TestEqual(TEXT("Run-up at t=0"), (ECricketBowlingAnimState)M.StateAtTime(0.0), ECricketBowlingAnimState::RunUp);
	TestEqual(TEXT("Release window at release time"), (ECricketBowlingAnimState)M.StateAtTime(T.ReleaseTimeSec()), ECricketBowlingAnimState::Release);
	return true;
}

// 3. BAT COLLISION TIMING: the BatImpact notify fires at the end of the downswing
//    (the contact instant) — the animation hook that coincides with the geometric
//    bat-ball contact the batting system resolves.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimImpactTest,
	"CricketSim.Anim.BatCollisionTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimImpactTest::RunTest(const FString&)
{
	const double Backlift = 0.18, Downswing = 0.16, Follow = 0.22, Dt = 0.005;
	const FCricketActionMontage M = FCricketAnimationModel::MakeBattingMontage(Backlift, Downswing, Follow);
	const TArray<FFired> F = PlayOut(M, Dt);

	TestEqual(TEXT("Impact fires once"), CountOf(F, ECricketAnimNotify::BatImpact), 1);
	const double FireT = TimeOf(F, ECricketAnimNotify::BatImpact);
	TestTrue(TEXT("Impact at end of downswing"),
		FireT >= Backlift + Downswing - 1e-9 && FireT <= Backlift + Downswing + Dt + 1e-9);

	TestEqual(TEXT("Phase mapping: Contact -> Impact"),
		FCricketAnimationModel::MapBattingPhase(ECricketSwingPhase::Contact), ECricketBattingAnimState::Impact);
	return true;
}

// 4. CATCH & THROW TIMING: the fielding montages fire their notifies on cue.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimFieldingTest,
	"CricketSim.Anim.CatchAndThrowTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimFieldingTest::RunTest(const FString&)
{
	const double Dt = 0.005;

	const FCricketActionMontage Catch = FCricketAnimationModel::MakeCatchMontage(0.25, 0.35);
	const TArray<FFired> CF = PlayOut(Catch, Dt);
	TestEqual(TEXT("One catch attempt"), CountOf(CF, ECricketAnimNotify::CatchAttempt), 1);
	TestTrue(TEXT("Catch attempt on reach"), FMath::Abs(TimeOf(CF, ECricketAnimNotify::CatchAttempt) - 0.25) <= Dt + 1e-9);

	const FCricketActionMontage Throw = FCricketAnimationModel::MakeThrowMontage(0.35, 0.45, 0.3);
	const TArray<FFired> TF = PlayOut(Throw, Dt);
	TestEqual(TEXT("One throw release"), CountOf(TF, ECricketAnimNotify::ThrowRelease), 1);
	TestTrue(TEXT("Throw release on cue"), FMath::Abs(TimeOf(TF, ECricketAnimNotify::ThrowRelease) - 0.45) <= Dt + 1e-9);

	const FCricketActionMontage Pick = FCricketAnimationModel::MakePickupMontage(0.30, 0.30);
	const TArray<FFired> PF = PlayOut(Pick, Dt);
	TestEqual(TEXT("One pickup contact"), CountOf(PF, ECricketAnimNotify::PickupContact), 1);
	return true;
}

// 5. GATHER PHASE: the back-foot gather/plant sits between the run-up and the
//    delivery stride, and lengthens the schedule release fires at by exactly
//    GatherTimeSec — proving the new phase is wired into the timeline, not just
//    declared.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimGatherTest,
	"CricketSim.Anim.BowlingGatherPhase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimGatherTest::RunTest(const FString&)
{
	FCricketBowlingActionTimeline T; // defaults (GatherTimeSec = 0.12)
	const FCricketActionMontage M = FCricketAnimationModel::MakeBowlingMontage(T);

	TestEqual(TEXT("Gather follows the run-up"),
		(ECricketBowlingAnimState)M.StateAtTime(T.RunUpTimeSec + T.GatherTimeSec * 0.5), ECricketBowlingAnimState::Gather);
	TestEqual(TEXT("Delivery stride follows the gather"),
		(ECricketBowlingAnimState)M.StateAtTime(T.RunUpTimeSec + T.GatherTimeSec + 0.01), ECricketBowlingAnimState::DeliveryStride);

	// Release time must include the gather: RunUp + Gather + ReleaseInStride.
	TestTrue(TEXT("Release time accounts for the gather"),
		FMath::IsNearlyEqual(T.ReleaseTimeSec(), T.RunUpTimeSec + T.GatherTimeSec + T.ReleaseInStrideSec, 1e-9));

	// Lengthening the gather pushes release back by exactly the same amount —
	// the timeline, not a hardcoded offset, owns the schedule.
	FCricketBowlingActionTimeline Longer = T;
	Longer.GatherTimeSec += 0.05;
	TestTrue(TEXT("Longer gather delays release by exactly its delta"),
		FMath::IsNearlyEqual(Longer.ReleaseTimeSec() - T.ReleaseTimeSec(), 0.05, 1e-9));

	const double Dt = 0.005;
	const TArray<FFired> F = PlayOut(M, Dt);
	const double FireT = TimeOf(F, ECricketAnimNotify::BallRelease);
	TestTrue(TEXT("Release still fires exactly on schedule with the gather present"),
		FireT >= T.ReleaseTimeSec() - 1e-9 && FireT <= T.ReleaseTimeSec() + Dt + 1e-9);
	return true;
}

// 6. PHYSICS-HANDOFF CLASSIFICATION: the notifies that gate/observe a physics
//    event are distinguished from purely cosmetic ones, for debug tooling.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimHandoffClassificationTest,
	"CricketSim.Anim.PhysicsHandoffClassification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimHandoffClassificationTest::RunTest(const FString&)
{
	using M = FCricketAnimationModel;
	TestTrue(TEXT("BallRelease is a handoff"), M::IsPhysicsHandoffNotify(ECricketAnimNotify::BallRelease));
	TestTrue(TEXT("BatImpact is a handoff"), M::IsPhysicsHandoffNotify(ECricketAnimNotify::BatImpact));
	TestTrue(TEXT("CatchAttempt is a handoff"), M::IsPhysicsHandoffNotify(ECricketAnimNotify::CatchAttempt));
	TestTrue(TEXT("PickupContact is a handoff"), M::IsPhysicsHandoffNotify(ECricketAnimNotify::PickupContact));
	TestTrue(TEXT("ThrowRelease is a handoff"), M::IsPhysicsHandoffNotify(ECricketAnimNotify::ThrowRelease));
	TestFalse(TEXT("FootPlant is cosmetic, not a handoff"), M::IsPhysicsHandoffNotify(ECricketAnimNotify::FootPlant));
	return true;
}

// 7. STATE TRANSITIONS: the bowling action moves through its states in order, and
//    the player advances through them deterministically.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAnimTransitionTest,
	"CricketSim.Anim.StateTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAnimTransitionTest::RunTest(const FString&)
{
	FCricketBowlingActionTimeline T;
	const FCricketActionMontage M = FCricketAnimationModel::MakeBowlingMontage(T);

	// Ordered states by time.
	TestEqual(TEXT("Starts in run-up"), (ECricketBowlingAnimState)M.StateAtTime(0.0), ECricketBowlingAnimState::RunUp);
	TestEqual(TEXT("Then gather"), (ECricketBowlingAnimState)M.StateAtTime(T.RunUpTimeSec + 0.01), ECricketBowlingAnimState::Gather);
	TestEqual(TEXT("Then delivery stride"), (ECricketBowlingAnimState)M.StateAtTime(T.RunUpTimeSec + T.GatherTimeSec + 0.01), ECricketBowlingAnimState::DeliveryStride);
	TestEqual(TEXT("Follows through after release"), (ECricketBowlingAnimState)M.StateAtTime(T.ReleaseTimeSec() + 0.1), ECricketBowlingAnimState::FollowThrough);
	TestEqual(TEXT("Ends in recover"), (ECricketBowlingAnimState)M.StateAtTime(T.TotalDurationSec() + 0.5), ECricketBowlingAnimState::Recover);

	// The player visits run-up, gather, stride and follow-through as it advances.
	FCricketMontagePlayer P; P.Start(M);
	bool bSawRunUp = false, bSawGather = false, bSawStride = false, bSawFollow = false;
	int32 Guard = 0;
	while (P.bPlaying && Guard++ < 200000)
	{
		TArray<ECricketAnimNotify> Fired;
		const ECricketBowlingAnimState St = (ECricketBowlingAnimState)P.CurrentStateId();
		bSawRunUp |= (St == ECricketBowlingAnimState::RunUp);
		bSawGather |= (St == ECricketBowlingAnimState::Gather);
		bSawStride |= (St == ECricketBowlingAnimState::DeliveryStride);
		bSawFollow |= (St == ECricketBowlingAnimState::FollowThrough);
		P.Advance(0.01, Fired);
	}
	TestTrue(TEXT("Visited run-up, gather, stride and follow-through"), bSawRunUp && bSawGather && bSawStride && bSawFollow);
	TestFalse(TEXT("Playback finished"), P.bPlaying);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
