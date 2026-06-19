// Headless automation tests for the broadcast PRESENTATION LAYER. Every decision core
// (event classifier, broadcast/replay directors, crowd & score models, match-flow
// sequences) is pure, so the whole "what to show, which camera, replay or not, how
// loud is the crowd, what does the score graphic read" logic is asserted with no
// world, no cameras, no RHI. The UCricketPresentationSubsystem playback wrapper itself
// is a thin driver over those cores and is exercised in PIE, not unit-tested here —
// the same split the UI and audio layers use.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Presentation; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#include "CricketPresentationTypes.h"
#include "CricketMatchSnapshot.h"
#include "CricketEventClassifier.h"
#include "CricketBroadcastDirector.h"
#include "CricketReplayDirector.h"
#include "CricketCrowdPresentationModel.h"
#include "CricketScorePresentationModel.h"
#include "CricketMatchFlowModel.h"
#include "CricketScoringTypes.h"      // FCricketDeliveryOutcome, ECricketDismissal
#include "CricketCameraTypes.h"       // ECricketCameraMode

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// A live-play snapshot the tests mutate per case.
	FCricketMatchSnapshot PresoSnap(int32 Runs, int32 Wkts, int32 LegalBalls)
	{
		FCricketMatchSnapshot S;
		S.bValid = true;
		S.State = ECricketMatchState::FirstInnings;
		S.BattingTeam = TEXT("India");
		S.BowlingTeam = TEXT("Australia");
		S.TeamRuns = Runs;
		S.TeamWickets = Wkts;
		S.LegalBalls = LegalBalls;
		return S;
	}
}

// 1. BOUNDARIES — a four and a six are distinct events with rising severity, carry the
//    striker, and are replay candidates.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationBoundaryTest,
	"CricketSim.Presentation.Boundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationBoundaryTest::RunTest(const FString&)
{
	FCricketMatchSnapshot After = PresoSnap(64, 1, 30);
	After.StrikerName = TEXT("Kohli");

	const FCricketPresentationEvent Four = FCricketEventClassifier::ClassifyBoundary(FCricketDeliveryOutcome::Four(), After);
	const FCricketPresentationEvent Six  = FCricketEventClassifier::ClassifyBoundary(FCricketDeliveryOutcome::Six(),  After);

	TestEqual(TEXT("Four -> Boundary event"), Four.Type, ECricketPresentationEventType::Boundary);
	TestEqual(TEXT("Six  -> Six event"),      Six.Type,  ECricketPresentationEventType::Six);
	TestTrue (TEXT("Six outranks four in severity"), Six.Severity > Four.Severity);
	TestEqual(TEXT("Four caption"), Four.Headline, FString(TEXT("FOUR!")));
	TestEqual(TEXT("Six caption"),  Six.Headline,  FString(TEXT("SIX!")));
	TestEqual(TEXT("Boundary carries the striker"), Four.PrimaryPlayer, FString(TEXT("Kohli")));
	TestTrue (TEXT("Boundaries are replay candidates"), Four.bReplayCandidate && Six.bReplayCandidate);
	TestTrue (TEXT("A six lifts the crowd more than a four"), Six.CrowdImpulse > Four.CrowdImpulse);

	// A six under chase pressure becomes a defining beat.
	FCricketMatchSnapshot Chase = PresoSnap(150, 3, 108);
	Chase.bChasing = true; Chase.RunsRequired = 10; Chase.BallsRemaining = 12;
	const FCricketPresentationEvent PressureSix = FCricketEventClassifier::ClassifyBoundary(FCricketDeliveryOutcome::Six(), Chase);
	TestEqual(TEXT("Pressure six is defining"), PressureSix.Severity, ECricketPresentationSeverity::Defining);
	return true;
}

// 2. WICKETS — the dismissal is named, the falling batter and the bowler are carried,
//    and a wicket always earns a replay.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationWicketTest,
	"CricketSim.Presentation.Wicket", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationWicketTest::RunTest(const FString&)
{
	FCricketMatchSnapshot Before = PresoSnap(40, 1, 24);
	Before.StrikerName = TEXT("Smith");
	Before.BowlerName  = TEXT("Bumrah");
	FCricketMatchSnapshot After = PresoSnap(40, 2, 25);
	After.BowlerName = TEXT("Bumrah");

	const FCricketPresentationEvent W = FCricketEventClassifier::ClassifyWicket(
		FCricketDeliveryOutcome::Out(ECricketDismissal::Bowled), Before, After);

	TestEqual(TEXT("Wicket event"), W.Type, ECricketPresentationEventType::Wicket);
	TestEqual(TEXT("Dismissal carried"), W.Dismissal, ECricketDismissal::Bowled);
	TestEqual(TEXT("Caption names the mode"), W.Headline, FString(TEXT("WICKET — bowled")));
	TestEqual(TEXT("Falling batter is the pre-ball striker"), W.PrimaryPlayer, FString(TEXT("Smith")));
	TestEqual(TEXT("Bowler is the secondary actor"), W.SecondaryPlayer, FString(TEXT("Bumrah")));
	TestTrue (TEXT("Wicket is a replay candidate"), W.bReplayCandidate);

	// A strike late in a tight chase is the highest severity.
	FCricketMatchSnapshot ChaseAfter = PresoSnap(150, 4, 110);
	ChaseAfter.bChasing = true; ChaseAfter.BallsRemaining = 10; ChaseAfter.BowlerName = TEXT("Starc");
	const FCricketPresentationEvent LateW = FCricketEventClassifier::ClassifyWicket(
		FCricketDeliveryOutcome::Out(ECricketDismissal::Caught), Before, ChaseAfter);
	TestEqual(TEXT("Late-chase wicket is defining"), LateW.Severity, ECricketPresentationSeverity::Defining);
	return true;
}

// 3. MILESTONES — fifty/century for the striker, a five-for for the bowler, and team
//    landmarks are recognised only when the matching tally actually crosses the mark.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationMilestoneTest,
	"CricketSim.Presentation.Milestone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationMilestoneTest::RunTest(const FString&)
{
	// Fifty.
	FCricketMatchSnapshot B = PresoSnap(120, 2, 80); B.StrikerName = TEXT("Kohli"); B.StrikerRuns = 48;
	FCricketMatchSnapshot A = PresoSnap(124, 2, 81); A.StrikerName = TEXT("Kohli"); A.StrikerRuns = 52;
	FCricketPresentationEvent Fifty = FCricketEventClassifier::ClassifyMilestone(B, A);
	TestEqual(TEXT("Crossing 50 -> fifty milestone"), Fifty.Milestone, ECricketMilestoneType::BatterFifty);
	TestEqual(TEXT("Fifty carries the batter"), Fifty.PrimaryPlayer, FString(TEXT("Kohli")));

	// Not crossing -> nothing.
	FCricketMatchSnapshot B2 = B; B2.StrikerRuns = 40;
	FCricketMatchSnapshot A2 = A; A2.StrikerRuns = 44;
	TestFalse(TEXT("No crossing -> no milestone"), FCricketEventClassifier::ClassifyMilestone(B2, A2).IsValid());

	// Bowler five-for (no batter/team crossing in play).
	FCricketMatchSnapshot Bb = PresoSnap(90, 4, 78); Bb.BowlerName = TEXT("Hazlewood"); Bb.BowlerWickets = 4; Bb.StrikerRuns = 12;
	FCricketMatchSnapshot Ab = PresoSnap(90, 5, 79); Ab.BowlerName = TEXT("Hazlewood"); Ab.BowlerWickets = 5; Ab.StrikerRuns = 12;
	FCricketPresentationEvent Five = FCricketEventClassifier::ClassifyMilestone(Bb, Ab);
	TestEqual(TEXT("Crossing 5 wkts -> five-for"), Five.Milestone, ECricketMilestoneType::BowlerFiveFor);

	// Team hundred (no individual crossing).
	FCricketMatchSnapshot Bt = PresoSnap(98, 3, 70); Bt.StrikerName = TEXT("Rahul"); Bt.StrikerRuns = 30; Bt.BowlerWickets = 1;
	FCricketMatchSnapshot At = PresoSnap(102, 3, 71); At.StrikerName = TEXT("Rahul"); At.StrikerRuns = 34; At.BowlerWickets = 1;
	FCricketPresentationEvent Hundred = FCricketEventClassifier::ClassifyMilestone(Bt, At);
	TestEqual(TEXT("Crossing team 100 -> team hundred"), Hundred.Milestone, ECricketMilestoneType::TeamHundred);
	return true;
}

// 4. FULL-BALL CLASSIFICATION + MATCH RESULT — priority ordering and the defining flag
//    when a single ball both seals the win and is a boundary.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationResultTest,
	"CricketSim.Presentation.Result", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationResultTest::RunTest(const FString&)
{
	FCricketMatchSnapshot Before = PresoSnap(175, 4, 116);
	Before.bChasing = true; Before.RunsRequired = 4; Before.BallsRemaining = 4; Before.StrikerName = TEXT("Maxwell");
	FCricketMatchSnapshot After = PresoSnap(181, 4, 117);
	After.bChasing = true; After.RunsRequired = 0; After.BallsRemaining = 3; After.StrikerName = TEXT("Maxwell");
	After.bResultDecided = true; After.WinningTeam = TEXT("Australia");
	After.ResultSummary = TEXT("Australia won by 6 wickets");

	const TArray<FCricketPresentationEvent> Events =
		FCricketEventClassifier::ClassifyDelivery(FCricketDeliveryOutcome::Six(), Before, After);

	TestTrue(TEXT("At least the six and the result"), Events.Num() >= 2);
	TestEqual(TEXT("Headline beat is the sealing six"), Events[0].Type, ECricketPresentationEventType::Six);
	TestTrue (TEXT("Sealing six is flagged match-defining"), Events[0].bMatchDefining);
	TestEqual(TEXT("Sealing six is defining severity"), Events[0].Severity, ECricketPresentationSeverity::Defining);
	TestEqual(TEXT("Last beat is the match result"), Events.Last().Type, ECricketPresentationEventType::MatchResult);
	TestEqual(TEXT("Result headline is the summary"), Events.Last().Headline, FString(TEXT("Australia won by 6 wickets")));

	// A decided match with no boundary/wicket still yields a single result beat.
	FCricketMatchSnapshot B2 = PresoSnap(180, 5, 119); B2.bResultDecided = false;
	FCricketMatchSnapshot A2 = PresoSnap(181, 5, 120); A2.bResultDecided = true; A2.WinningTeam = TEXT("India"); A2.ResultSummary = TEXT("India won by 5 wickets");
	const TArray<FCricketPresentationEvent> Won = FCricketEventClassifier::ClassifyDelivery(FCricketDeliveryOutcome::Runs(1), B2, A2);
	TestEqual(TEXT("Winning single -> one result beat"), Won.Num(), 1);
	TestEqual(TEXT("It is the match result"), Won[0].Type, ECricketPresentationEventType::MatchResult);
	return true;
}

// 5. BROADCAST DIRECTOR — live-camera hysteresis, per-event angle choice, and the
//    mapping onto the existing gameplay camera modes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationBroadcastTest,
	"CricketSim.Presentation.Broadcast", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationBroadcastTest::RunTest(const FString&)
{
	FCricketBroadcastDirector Dir;
	Dir.Current = ECricketBroadcastCamera::MainBroadcast;
	Dir.MinHoldSeconds = 0.75f;

	// Between balls (bowler at the mark) cuts to the bowling angle after the min hold.
	Dir.SelectLiveCamera(/*waiting*/ true, /*inflight*/ false, /*rope*/ false, 0.3f);
	TestEqual(TEXT("Held below minimum -> no cut yet"), Dir.Current, ECricketBroadcastCamera::MainBroadcast);
	Dir.SelectLiveCamera(true, false, false, 0.6f);
	TestEqual(TEXT("Past the hold -> bowling angle"), Dir.Current, ECricketBroadcastCamera::Bowling);

	// A ball racing to the rope is urgent and cuts immediately.
	const ECricketBroadcastCamera Rope = Dir.SelectLiveCamera(false, true, /*rope*/ true, 0.05f);
	TestEqual(TEXT("Boundary chase cuts immediately"), Rope, ECricketBroadcastCamera::Boundary);

	// Event angles.
	FCricketPresentationEvent Bowled; Bowled.Type = ECricketPresentationEventType::Wicket; Bowled.Dismissal = ECricketDismissal::Bowled;
	FCricketPresentationEvent Caught; Caught.Type = ECricketPresentationEventType::Wicket; Caught.Dismissal = ECricketDismissal::Caught;
	FCricketPresentationEvent Six;    Six.Type    = ECricketPresentationEventType::Six;
	FCricketPresentationEvent Mile;   Mile.Type   = ECricketPresentationEventType::Milestone;
	TestEqual(TEXT("Bowled -> stump cam"), FCricketBroadcastDirector::SelectCameraForEvent(Bowled), ECricketBroadcastCamera::Stump);
	TestEqual(TEXT("Caught -> master"),    FCricketBroadcastDirector::SelectCameraForEvent(Caught), ECricketBroadcastCamera::MainBroadcast);
	TestEqual(TEXT("Six -> boundary cam"), FCricketBroadcastDirector::SelectCameraForEvent(Six),    ECricketBroadcastCamera::Boundary);
	TestEqual(TEXT("Milestone -> batting"),FCricketBroadcastDirector::SelectCameraForEvent(Mile),   ECricketBroadcastCamera::Batting);

	// Mapping onto gameplay camera modes.
	TestEqual(TEXT("Main -> Spectator"),  FCricketBroadcastDirector::CameraModeFor(ECricketBroadcastCamera::MainBroadcast), ECricketCameraMode::Spectator);
	TestEqual(TEXT("Stump -> BallFollow"),FCricketBroadcastDirector::CameraModeFor(ECricketBroadcastCamera::Stump),         ECricketCameraMode::BallFollow);
	TestEqual(TEXT("Replay -> Orbit"),    FCricketBroadcastDirector::CameraModeFor(ECricketBroadcastCamera::Replay),        ECricketCameraMode::Orbit);
	return true;
}

// 6. REPLAY DIRECTOR — wickets and sixes always replay (multi-angle, slow), a low-key
//    four does not, the match result gets the fullest treatment.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationReplayTest,
	"CricketSim.Presentation.Replay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationReplayTest::RunTest(const FString&)
{
	FCricketReplayDirector Dir;

	FCricketPresentationEvent Bowled; Bowled.Type = ECricketPresentationEventType::Wicket;
	Bowled.Dismissal = ECricketDismissal::Bowled; Bowled.Severity = ECricketPresentationSeverity::High; Bowled.bReplayCandidate = true;
	const FCricketReplayPlan WPlan = Dir.BuildPlan(Bowled);
	TestTrue (TEXT("Wicket replays"), WPlan.bShouldReplay);
	TestTrue (TEXT("Wicket is slow motion"), WPlan.SlowMoRate < 1.0f);
	TestTrue (TEXT("Wicket is multi-angle"), WPlan.NumAngles() >= 2);
	TestEqual(TEXT("Bowled leads with the stump cam"), WPlan.Angles[0], ECricketBroadcastCamera::Stump);

	// A modest four does not earn an automatic replay; a pressure four does.
	FCricketPresentationEvent SoftFour; SoftFour.Type = ECricketPresentationEventType::Boundary;
	SoftFour.Severity = ECricketPresentationSeverity::Medium; SoftFour.bReplayCandidate = true;
	TestFalse(TEXT("A low-key four does not replay"), Dir.BuildPlan(SoftFour).bShouldReplay);

	FCricketPresentationEvent BigFour = SoftFour; BigFour.Severity = ECricketPresentationSeverity::High;
	TestTrue (TEXT("A pressure four replays"), Dir.BuildPlan(BigFour).bShouldReplay);

	// A six replays from the boundary first.
	FCricketPresentationEvent Six; Six.Type = ECricketPresentationEventType::Six; Six.Severity = ECricketPresentationSeverity::High; Six.bReplayCandidate = true;
	const FCricketReplayPlan SixPlan = Dir.BuildPlan(Six);
	TestTrue (TEXT("Six replays"), SixPlan.bShouldReplay);
	TestEqual(TEXT("Six leads from the boundary"), SixPlan.Angles[0], ECricketBroadcastCamera::Boundary);

	// The match result gets the slowest, longest, multi-angle package.
	FCricketPresentationEvent Res; Res.Type = ECricketPresentationEventType::MatchResult;
	Res.Severity = ECricketPresentationSeverity::Defining; Res.bReplayCandidate = true; Res.bMatchDefining = true;
	const FCricketReplayPlan RPlan = Dir.BuildPlan(Res);
	TestTrue (TEXT("Result replays"), RPlan.bShouldReplay);
	TestTrue (TEXT("Result is the slowest"), RPlan.SlowMoRate <= WPlan.SlowMoRate);
	TestTrue (TEXT("Result is multi-angle"), RPlan.NumAngles() >= 3);

	// A non-candidate (a milestone graphic) never rolls a ball replay.
	FCricketPresentationEvent Mile; Mile.Type = ECricketPresentationEventType::Milestone; Mile.bReplayCandidate = false;
	TestFalse(TEXT("Milestone does not replay the ball"), Dir.BuildPlan(Mile).bShouldReplay);
	return true;
}

// 7. CROWD ATMOSPHERE ARC — an event lifts the crowd, a tight death-overs chase sets a
//    high baseline that reads as Tense, and the charge decays back toward calm.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationCrowdTest,
	"CricketSim.Presentation.Crowd", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationCrowdTest::RunTest(const FString&)
{
	// Event impulse lifts the atmosphere.
	FCricketCrowdPresentationModel C;
	FCricketPresentationEvent Six; Six.Type = ECricketPresentationEventType::Six; Six.CrowdImpulse = 0.6f;
	C.ApplyEvent(Six);
	C.Tick(0.2f);
	TestTrue(TEXT("A six lifts the crowd above resting"), C.Atmosphere > 0.0f);

	// A tight chase in the death overs is a sustained Tense atmosphere.
	FCricketCrowdPresentationModel Tense;
	FCricketMatchSnapshot Death = PresoSnap(150, 4, 108);
	Death.bChasing = true; Death.RunsRequired = 15; Death.BallsRemaining = 9;
	Tense.UpdateContext(Death);
	TestTrue(TEXT("Close chase raises the baseline"), Tense.ContextBaseline > 0.6f);
	for (int32 i = 0; i < 12; ++i) { Tense.Tick(0.5f); }
	TestTrue (TEXT("Atmosphere converges high"), Tense.Atmosphere > 0.45f);
	TestEqual(TEXT("Mood reads Tense in a close finish"), Tense.Mood(), ECricketCrowdMood::Tense);

	// A one-sided game settles calm.
	FCricketCrowdPresentationModel Calm;
	FCricketMatchSnapshot Easy = PresoSnap(60, 1, 60); // first innings, not chasing
	Calm.UpdateContext(Easy);
	for (int32 i = 0; i < 10; ++i) { Calm.Tick(0.5f); }
	TestTrue(TEXT("A quiet passage relaxes toward calm"), Calm.Atmosphere < 0.3f);
	TestEqual(TEXT("Mood is Calm"), Calm.Mood(), ECricketCrowdMood::Calm);
	return true;
}

// 8. SCORE GRAPHICS — over summary at the end of the over, the partnership grows and
//    resets on a wicket, the chase line and milestone notifications read correctly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationScoreTest,
	"CricketSim.Presentation.Score", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationScoreTest::RunTest(const FString&)
{
	FCricketScorePresentationModel M;
	M.ResetInnings();

	// Six legal balls of one run each completes over 1 with a fresh summary.
	bool bOverDone = false;
	for (int32 b = 1; b <= 6; ++b)
	{
		const FCricketMatchSnapshot Before = PresoSnap(b - 1, 0, b - 1);
		const FCricketMatchSnapshot After  = PresoSnap(b, 0, b);
		bOverDone = M.OnBall(Before, After);
	}
	TestTrue(TEXT("Sixth ball completes the over"), bOverDone);
	TestTrue(TEXT("Over summary mentions over 1"), M.LastOverSummary.Contains(TEXT("Over 1")));
	TestTrue(TEXT("Over summary mentions the score"), M.LastOverSummary.Contains(TEXT("6/0")));
	TestEqual(TEXT("Partnership grew to 6 (6)"), M.PartnershipText(), FString(TEXT("Partnership 6 (6)")));

	// A wicket breaks the stand: the partnership resets.
	M.OnBall(PresoSnap(6, 0, 6), PresoSnap(6, 1, 7));
	TestEqual(TEXT("Partnership resets on a wicket"), M.PartnershipText(), FString(TEXT("Partnership 0 (0)")));

	// Chase line.
	FCricketMatchSnapshot Chase = PresoSnap(133, 4, 90);
	Chase.bChasing = true; Chase.RunsRequired = 48; Chase.BallsRemaining = 30; Chase.RequiredRunRate = 9.6;
	TestEqual(TEXT("Chase line"), FCricketScorePresentationModel::ChaseText(Chase), FString(TEXT("Need 48 from 30 • RRR 9.60")));
	TestTrue (TEXT("No chase line in the first innings"), FCricketScorePresentationModel::ChaseText(PresoSnap(80, 2, 60)).IsEmpty());

	// Milestone notification.
	FCricketPresentationEvent Fifty; Fifty.Type = ECricketPresentationEventType::Milestone;
	Fifty.Milestone = ECricketMilestoneType::BatterFifty; Fifty.PrimaryPlayer = TEXT("Kohli");
	FCricketMatchSnapshot FiftyAfter = PresoSnap(124, 2, 81); FiftyAfter.StrikerName = TEXT("Kohli"); FiftyAfter.StrikerBalls = 34;
	TestEqual(TEXT("Fifty notification"), FCricketScorePresentationModel::MilestoneText(Fifty, FiftyAfter),
		FString(TEXT("FIFTY • Kohli (34 balls)")));
	return true;
}

// 9. MATCH-FLOW SEQUENCES — every broadcast package builds non-empty, captioned, timed
//    steps, and the runtime cursor advances and completes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPresentationFlowTest,
	"CricketSim.Presentation.Flow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPresentationFlowTest::RunTest(const FString&)
{
	const FCricketBroadcastSequence Intro = FCricketMatchFlowModel::BuildMatchIntro(TEXT("India"), TEXT("Australia"), 20);
	TestTrue(TEXT("Intro has steps"), Intro.NumSteps() >= 2);
	TestTrue(TEXT("Intro names the teams"), Intro.Steps[0].Caption.Contains(TEXT("INDIA")) && Intro.Steps[0].Caption.Contains(TEXT("AUSTRALIA")));
	TestTrue(TEXT("Intro has a positive duration"), Intro.TotalDuration() > 0.0f);

	const FCricketBroadcastSequence Toss = FCricketMatchFlowModel::BuildToss(TEXT("Australia"), /*bChoseToBat*/ false);
	bool bFoundBowl = false;
	for (const FCricketBroadcastStep& S : Toss.Steps) { if (S.Caption.Contains(TEXT("chose to bowl"))) { bFoundBowl = true; } }
	TestTrue(TEXT("Toss states the decision"), bFoundBowl);

	const FCricketBroadcastSequence Break = FCricketMatchFlowModel::BuildInningsTransition(TEXT("India"), 180, 6, 181);
	bool bFoundTarget = false;
	for (const FCricketBroadcastStep& S : Break.Steps) { if (S.Caption.Contains(TEXT("Target: 181"))) { bFoundTarget = true; } }
	TestTrue(TEXT("Innings break shows the target"), bFoundTarget);

	FCricketMatchSnapshot Final; Final.bValid = true; Final.bResultDecided = true;
	Final.WinningTeam = TEXT("India"); Final.ResultSummary = TEXT("India won by 5 wickets");
	const FCricketBroadcastSequence Result = FCricketMatchFlowModel::BuildMatchResult(Final);
	bool bFoundSummary = false;
	for (const FCricketBroadcastStep& S : Result.Steps) { if (S.Caption.Contains(TEXT("India won by 5 wickets"))) { bFoundSummary = true; } }
	TestTrue(TEXT("Result shows the summary"), bFoundSummary);

	// Cursor advances through the steps and finishes.
	FCricketBroadcastSequence Seq = Intro;
	Seq.Begin();
	TestEqual(TEXT("Starts on step 0"), Seq.CurrentStep, 0);
	const bool bMid = Seq.Advance(Seq.Steps[0].DurationSeconds + 0.01f);
	TestTrue (TEXT("Still running mid-sequence"), bMid);
	TestTrue (TEXT("Advanced past the first step"), Seq.CurrentStep >= 1);
	const bool bRunning = Seq.Advance(Seq.TotalDuration() + 1.0f);
	TestFalse(TEXT("Sequence completes"), bRunning);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
