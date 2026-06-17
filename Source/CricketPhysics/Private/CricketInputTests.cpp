// Headless automation tests for the control layer's pure brain: the Cricket-07
// key-combo -> intent mapping (batting/bowling/running/fielding) and the input
// context switching. Controls only generate intent; these tests prove the mapping,
// not any outcome.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Input; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketInputModel.h"
#include "CricketInputTypes.h"
#include "CricketBattingTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

using M = FCricketInputModel;

namespace
{
	FCricketBattingControlState Bat(bool bFront, bool bBack, bool bDef, bool bLoft, ECricketShotDirection Dir)
	{
		FCricketBattingControlState S;
		S.bFrontFoot = bFront; S.bBackFoot = bBack; S.bDefensive = bDef; S.bLofted = bLoft; S.Direction = Dir;
		return S;
	}
}

// 1. BATTING COMBINATIONS: the Cricket-07 grid (foot x direction x modifier).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketInputBattingTest,
	"CricketSim.Input.BattingCombinations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketInputBattingTest::RunTest(const FString&)
{
	// S -> defensive block, never lofted.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(false, false, true, true, ECricketShotDirection::OffSide));
		TestEqual(TEXT("S -> Defensive"), I.Shot, ECricketC07Shot::Defensive);
		TestFalse(TEXT("Cannot loft a block"), I.bLofted);
		TestEqual(TEXT("Maps to DefensiveBlock"), M::ToBattingInput(I, true).ShotType, ECricketShotType::DefensiveBlock);
	}
	// D + off -> front-foot cover drive aimed to the off side.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(true, false, false, false, ECricketShotDirection::CoverRegion));
		TestEqual(TEXT("D + off -> Cover Drive"), I.Shot, ECricketC07Shot::CoverDrive);
		TestEqual(TEXT("Front foot"), I.Footwork, ECricketFootwork::FrontFoot);
		const FCricketBattingInput In = M::ToBattingInput(I, true);
		TestEqual(TEXT("-> CoverDrive"), In.ShotType, ECricketShotType::CoverDrive);
		TestTrue(TEXT("Aimed off side (+)"), In.AimYawDeg > 0.0);
	}
	// W + leg -> back-foot pull aimed to leg.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(false, true, false, false, ECricketShotDirection::LegSide));
		TestEqual(TEXT("W + leg -> Pull"), I.Shot, ECricketC07Shot::PullShot);
		TestEqual(TEXT("Back foot"), I.Footwork, ECricketFootwork::BackFoot);
		TestTrue(TEXT("Aimed leg side (-)"), M::ToBattingInput(I, true).AimYawDeg < 0.0);
	}
	// W + off -> cut shot.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(false, true, false, false, ECricketShotDirection::OffSide));
		TestEqual(TEXT("W + off -> Cut"), I.Shot, ECricketC07Shot::CutShot);
		const FCricketBattingInput In = M::ToBattingInput(I, true);
		TestEqual(TEXT("Cut uses back foot"), In.Footwork, ECricketFootwork::BackFoot);
		TestTrue(TEXT("Cut is square of the wicket (off)"), In.AimYawDeg >= 35.0);
	}
	// D + leg -> flick.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(true, false, false, false, ECricketShotDirection::MidwicketRegion));
		TestEqual(TEXT("D + leg -> Flick"), I.Shot, ECricketC07Shot::FlickShot);
		TestTrue(TEXT("Flick is to the leg side"), M::ToBattingInput(I, true).AimYawDeg < 0.0);
	}
	// Shift + D + straight -> lofted drive, more power.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(true, false, false, true, ECricketShotDirection::Straight));
		TestEqual(TEXT("Shift + drive -> Lofted Drive"), I.Shot, ECricketC07Shot::LoftedDrive);
		TestTrue(TEXT("Lofted swings harder"), I.PowerScale > 1.1);
		TestTrue(TEXT("Lofted flag set"), I.bLofted);
	}
	// Left-hander mirrors the off-side aim to -Y.
	{
		const FCricketBattingShotIntent I = M::ResolveBattingShot(Bat(true, false, false, false, ECricketShotDirection::OffSide));
		TestTrue(TEXT("Off aim is +ve for RH"), M::ToBattingInput(I, true).AimYawDeg > 0.0);
		// (Y mirroring of the actual stroke happens in the swing model via bRightHanded.)
		TestFalse(TEXT("LH input flags right-handed off"), M::ToBattingInput(I, false).bRightHanded);
	}
	return true;
}

// 2. BOWLING COMBINATIONS: delivery choice + line/length + swing/spin modifiers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketInputBowlingTest,
	"CricketSim.Input.BowlingCombinations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketInputBowlingTest::RunTest(const FString&)
{
	FCricketBowlingControlState S;
	S.Delivery = ECricketDeliveryChoice::Aggressive; S.LineStep = 1; S.LengthStep = -1; S.bSwingMod = true;
	const FCricketBowlingControlIntent A = M::ResolveDelivery(S);
	TestTrue(TEXT("Aggressive is quickest"), A.Pace01 >= 1.0);
	TestEqual(TEXT("Line steps to off"), A.LineStepDir, 1);
	TestEqual(TEXT("Length steps fuller"), A.LengthStepDir, -1);
	TestTrue(TEXT("Swing modifier raises swing"), A.SwingAmount > 0.8);

	FCricketBowlingControlState Slow; Slow.Delivery = ECricketDeliveryChoice::Variation; Slow.bSpinMod = true;
	const FCricketBowlingControlIntent V = M::ResolveDelivery(Slow);
	TestTrue(TEXT("Variation is slower than stock"), V.Pace01 < M::ResolveDelivery(FCricketBowlingControlState()).Pace01);
	TestTrue(TEXT("Spin modifier raises spin"), V.SpinAmount > 0.8);
	return true;
}

// 3. RUNNING ACTIONS: D take, A send back, W dive (with priority).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketInputRunningTest,
	"CricketSim.Input.RunningActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketInputRunningTest::RunTest(const FString&)
{
	TestEqual(TEXT("D -> Take"), M::ResolveRunCall(true, false, false), ECricketRunCall::Take);
	TestEqual(TEXT("A -> Send Back"), M::ResolveRunCall(false, true, false), ECricketRunCall::SendBack);
	TestEqual(TEXT("W -> Dive"), M::ResolveRunCall(false, false, true), ECricketRunCall::Dive);
	TestEqual(TEXT("Dive overrides a take"), M::ResolveRunCall(true, false, true), ECricketRunCall::Dive);
	TestEqual(TEXT("Nothing held -> None"), M::ResolveRunCall(false, false, false), ECricketRunCall::None);
	return true;
}

// 4. FIELDING ACTIONS: catch/dive/throw/relay/move with priority.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketInputFieldingTest,
	"CricketSim.Input.FieldingActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketInputFieldingTest::RunTest(const FString&)
{
	TestEqual(TEXT("Catch first"), M::ResolveFieldAction(true, true, true, false, true), ECricketFieldAction::Catch);
	TestEqual(TEXT("Dive over throw"), M::ResolveFieldAction(false, true, true, false, false), ECricketFieldAction::Dive);
	TestEqual(TEXT("Throw"), M::ResolveFieldAction(false, false, true, false, false), ECricketFieldAction::Throw);
	TestEqual(TEXT("Relay"), M::ResolveFieldAction(false, false, false, true, false), ECricketFieldAction::RelayThrow);
	TestEqual(TEXT("Move"), M::ResolveFieldAction(false, false, false, false, true), ECricketFieldAction::Move);
	TestEqual(TEXT("None"), M::ResolveFieldAction(false, false, false, false, false), ECricketFieldAction::None);
	return true;
}

// 5. INPUT CONTEXT SWITCHING: the active layer follows the role; replay overrides.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketInputContextTest,
	"CricketSim.Input.ContextSwitching", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketInputContextTest::RunTest(const FString&)
{
	TestEqual(TEXT("Batting role -> Batting"), M::ResolveContext(false, true, false, false), ECricketInputContext::Batting);
	TestEqual(TEXT("Bowling role -> Bowling"), M::ResolveContext(false, false, true, false), ECricketInputContext::Bowling);
	TestEqual(TEXT("Fielding role -> Fielding"), M::ResolveContext(false, false, false, true), ECricketInputContext::Fielding);
	TestEqual(TEXT("No role -> Match"), M::ResolveContext(false, false, false, false), ECricketInputContext::Match);
	TestEqual(TEXT("Replay overrides batting"), M::ResolveContext(true, true, false, false), ECricketInputContext::Replay);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
