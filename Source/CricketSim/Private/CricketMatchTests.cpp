// Headless automation tests for the T20 match engine (the rules layer). The engine
// consumes interpreted ball outcomes and applies cricket law; these tests feed it
// scorebook outcomes directly and assert the laws — no physics, no randomness.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Match; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketMatchEngine.h"
#include "CricketOutcomeInterpreter.h"
#include "CricketScoringTypes.h"
#include "CricketMatchTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using FOutcome = FCricketDeliveryOutcome;

	FCricketSquad MakeSquad(const FString& Team, const FString& Code)
	{
		FCricketSquad S; S.TeamName = Team; S.ShortCode = Code;
		for (int32 i = 0; i < 11; ++i) { S.PlayerNames.Add(FString::Printf(TEXT("%s%d"), *Code, i)); }
		return S;
	}

	// A configured engine with India & Australia, OversPerInnings overridable, with
	// India batting first (Australia bowling). Returns it in FirstInnings.
	UCricketMatchEngine* NewTossed(int32 Overs = 20)
	{
		UCricketMatchEngine* E = NewObject<UCricketMatchEngine>();
		FCricketMatchRules R; R.OversPerInnings = Overs;
		E->ConfigureMatch(R, MakeSquad(TEXT("India"), TEXT("IND")), MakeSquad(TEXT("Australia"), TEXT("AUS")));
		E->StartMatch();
		E->PerformToss(0, /*bWinnerBatsFirst*/ true); // India bat first
		return E;
	}

	// Bowler pool that never bowls consecutive overs and stays within the cap.
	FString PoolBowler(const FString& Code, int32 OverIndex) { return FString::Printf(TEXT("%s%d"), *Code, OverIndex % 5); }
}

// 1. FULL OVER: six legal balls complete an over, runs tally, a maiden is a maiden,
//    and the strike rotates between overs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchFullOverTest,
	"CricketSim.Match.FullOver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchFullOverTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed();
	TestTrue(TEXT("Bowler set"), E->SetBowler(TEXT("AUS0")));

	// A scoring over: 1, 4, 0, 6, 2, 1 = 14.
	E->ApplyDelivery(FOutcome::Runs(1));
	E->ApplyDelivery(FOutcome::Four());
	E->ApplyDelivery(FOutcome::Dot());
	E->ApplyDelivery(FOutcome::Six());
	E->ApplyDelivery(FOutcome::Runs(2));
	E->ApplyDelivery(FOutcome::Runs(1));

	const FCricketInningsScorecard& A = E->GetActiveInnings();
	TestEqual(TEXT("Over complete"), A.CompletedOvers(), 1);
	TestEqual(TEXT("No balls into the next over"), A.BallsThisOver(), 0);
	TestEqual(TEXT("Runs tallied"), A.Totals.Runs, 14);
	TestEqual(TEXT("Bowler conceded the lot"), A.Bowlers[0].RunsConceded, 14);
	TestEqual(TEXT("Six legal balls bowled"), A.Bowlers[0].LegalBalls, 6);
	TestEqual(TEXT("A four recorded"), A.Batters[A.NonStrikerIndex].Fours + A.Batters[A.StrikerIndex].Fours, 1);
	return true;
}

// 1b. A maiden over: six dots, no runs charged, strike rotates once at the end.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchMaidenTest,
	"CricketSim.Match.MaidenAndStrike", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchMaidenTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed();
	E->SetBowler(TEXT("AUS0"));
	const FString Opener = E->GetStrikerName();

	for (int32 i = 0; i < 6; ++i) { E->ApplyDelivery(FOutcome::Dot()); }

	const FCricketInningsScorecard& A = E->GetActiveInnings();
	TestEqual(TEXT("Maiden recorded"), A.Bowlers[0].Maidens, 1);
	TestEqual(TEXT("No runs"), A.Totals.Runs, 0);
	TestNotEqual(TEXT("Strike rotated at the over's end"), E->GetStrikerName(), Opener);

	// A single mid-over rotates the strike immediately.
	E->SetBowler(TEXT("AUS1"));
	const FString S0 = E->GetStrikerName();
	E->ApplyDelivery(FOutcome::Runs(1));
	TestNotEqual(TEXT("Single rotates strike"), E->GetStrikerName(), S0);
	return true;
}

// 2. EXTRAS: wides/no-balls add a penalty and are re-bowled; byes are extras not
//    charged to the bowler or the batter.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchExtrasTest,
	"CricketSim.Match.Extras", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchExtrasTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed();
	E->SetBowler(TEXT("AUS0"));

	E->ApplyDelivery(FOutcome::Wide());        // +1, re-bowled
	E->ApplyDelivery(FOutcome::NoBall(4));      // +1 penalty + 4 off the bat, re-bowled
	E->ApplyDelivery(FOutcome::Bye(2));         // legal, +2 byes

	const FCricketInningsScorecard& A = E->GetActiveInnings();
	TestEqual(TEXT("Total = 1 + 5 + 2"), A.Totals.Runs, 8);
	TestEqual(TEXT("Extras = wide + no-ball + byes"), A.Totals.Extras, 1 + 1 + 2);
	TestEqual(TEXT("Only the bye was a legal ball"), A.Totals.LegalBalls, 1);
	TestEqual(TEXT("Bowler charged penalty + bat (not byes)"), A.Bowlers[0].RunsConceded, 1 + 5);
	// The striker took the 4 off the no-ball but faced only the (bye) legal ball.
	TestEqual(TEXT("Striker has the no-ball four"), A.Batters[0].Runs, 4);
	TestEqual(TEXT("Striker faced one legal ball"), A.Batters[0].Balls, 1);
	return true;
}

// 3. WICKETS: caught is credited to the bowler, run out is not; the next batter
//    comes in; an innings ends when all out.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchWicketTest,
	"CricketSim.Match.Wickets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchWicketTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed(2);
	E->SetBowler(TEXT("AUS0"));

	E->ApplyDelivery(FOutcome::Out(ECricketDismissal::Caught));
	{
		const FCricketInningsScorecard& A = E->GetActiveInnings();
		TestEqual(TEXT("One wicket"), A.Totals.Wickets, 1);
		TestTrue(TEXT("Opener is out caught"), A.Batters[0].bOut && A.Batters[0].Dismissal == ECricketDismissal::Caught);
		TestEqual(TEXT("Caught credited to the bowler"), A.Bowlers[0].Wickets, 1);
		TestEqual(TEXT("New batter (#3) is on strike"), E->GetStrikerName(), FString(TEXT("IND2")));
	}

	E->ApplyDelivery(FOutcome::Out(ECricketDismissal::RunOut));
	{
		const FCricketInningsScorecard& A = E->GetActiveInnings();
		TestEqual(TEXT("Two wickets"), A.Totals.Wickets, 2);
		TestEqual(TEXT("Run out NOT credited to the bowler"), A.Bowlers[0].Wickets, 1);
	}

	// Knock the rest over to force all out (10 wickets) within the 2 overs.
	for (int32 i = 0; i < 4; ++i) { E->ApplyDelivery(FOutcome::Out(ECricketDismissal::Bowled)); } // wickets 3..6, over ends at 6 balls
	E->SetBowler(TEXT("AUS1"));
	for (int32 i = 0; i < 4; ++i) { E->ApplyDelivery(FOutcome::Out(ECricketDismissal::Bowled)); } // wickets 7..10

	TestTrue(TEXT("All out ends the innings"), E->GetInnings(0).bAllOut);
	TestEqual(TEXT("First innings closed -> innings break"), (int32)E->GetMatchState(), (int32)ECricketMatchState::InningsBreak);
	return true;
}

// 4. INNINGS TRANSITIONS: a completed first innings sets the target and moves to
//    the break, then the second innings starts as the chase.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchTransitionTest,
	"CricketSim.Match.InningsTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchTransitionTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed(2); // 2-over innings

	TestEqual(TEXT("Starts in the first innings"), (int32)E->GetMatchState(), (int32)ECricketMatchState::FirstInnings);

	// Two overs of singles = 12 runs.
	for (int32 over = 0; over < 2; ++over)
	{
		TestTrue(TEXT("Bowler set"), E->SetBowler(PoolBowler(TEXT("AUS"), over)));
		for (int32 b = 0; b < 6; ++b) { E->ApplyDelivery(FOutcome::Runs(1)); }
	}

	TestEqual(TEXT("First innings runs"), E->GetInnings(0).Totals.Runs, 12);
	TestEqual(TEXT("Now at the innings break"), (int32)E->GetMatchState(), (int32)ECricketMatchState::InningsBreak);
	TestEqual(TEXT("Target is runs + 1"), E->GetTarget(), 13);

	E->StartSecondInnings();
	TestEqual(TEXT("Second innings underway"), (int32)E->GetMatchState(), (int32)ECricketMatchState::SecondInnings);
	TestEqual(TEXT("Australia now batting"), E->GetActiveInnings().BattingTeam, FString(TEXT("Australia")));
	TestEqual(TEXT("Need 13 to win"), E->RunsRequired(), 13);
	return true;
}

// 5. CHASE: the chasing side passes the target and wins by wickets, ending the match.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchChaseTest,
	"CricketSim.Match.Chase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchChaseTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed(1); // 1-over match
	E->SetBowler(TEXT("AUS0"));
	for (int32 b = 0; b < 6; ++b) { E->ApplyDelivery(FOutcome::Runs(1)); } // India 6
	TestEqual(TEXT("Target 7"), E->GetTarget(), 7);

	E->StartSecondInnings();
	E->SetBowler(TEXT("IND0"));
	E->ApplyDelivery(FOutcome::Four());
	E->ApplyDelivery(FOutcome::Four()); // 8 >= 7 -> chase done

	TestEqual(TEXT("Match complete"), (int32)E->GetMatchState(), (int32)ECricketMatchState::MatchComplete);
	const FCricketMatchResult& R = E->GetResult();
	TestTrue(TEXT("Result decided"), R.bDecided && !R.bTie);
	TestEqual(TEXT("Australia won"), R.WinningTeam, FString(TEXT("Australia")));
	TestTrue(TEXT("Won by wickets"), R.Summary.Contains(TEXT("wicket")));
	return true;
}

// 6. TIE: the chase finishes level with the target-1 -> a tie, match complete.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchTieTest,
	"CricketSim.Match.Tie", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchTieTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed(1);
	E->SetBowler(TEXT("AUS0"));
	for (int32 b = 0; b < 6; ++b) { E->ApplyDelivery(FOutcome::Runs(1)); } // India 6, target 7

	E->StartSecondInnings();
	E->SetBowler(TEXT("IND0"));
	for (int32 b = 0; b < 6; ++b) { E->ApplyDelivery(FOutcome::Runs(1)); } // Australia 6 = target-1

	TestEqual(TEXT("Match complete"), (int32)E->GetMatchState(), (int32)ECricketMatchState::MatchComplete);
	TestTrue(TEXT("It is a tie"), E->GetResult().bTie);
	TestEqual(TEXT("Tie summary"), E->GetResult().Summary, FString(TEXT("Match tied")));
	return true;
}

// 7. MATCH COMPLETION by defending: the chase falls short and the bowling side wins
//    by runs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchDefendTest,
	"CricketSim.Match.DefendWin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchDefendTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed(1);
	E->SetBowler(TEXT("AUS0"));
	E->ApplyDelivery(FOutcome::Six());
	for (int32 b = 0; b < 5; ++b) { E->ApplyDelivery(FOutcome::Runs(1)); } // India 11, target 12

	E->StartSecondInnings();
	E->SetBowler(TEXT("IND0"));
	for (int32 b = 0; b < 6; ++b) { E->ApplyDelivery(FOutcome::Runs(1)); } // Australia 6

	TestEqual(TEXT("Match complete"), (int32)E->GetMatchState(), (int32)ECricketMatchState::MatchComplete);
	const FCricketMatchResult& R = E->GetResult();
	TestEqual(TEXT("India defended"), R.WinningTeam, FString(TEXT("India")));
	TestTrue(TEXT("Won by 5 runs"), R.Summary.Contains(TEXT("by 5 runs")));
	return true;
}

// 8. INTERPRETER: the consume-physics seam classifies raw ball facts into the
//    correct scorebook outcome (it interprets, never alters).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketMatchInterpreterTest,
	"CricketSim.Match.Interpreter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketMatchInterpreterTest::RunTest(const FString&)
{
	// Cleared the rope -> six.
	{
		FCricketBallResult R; R.bStruck = true; R.bBoundarySix = true;
		const FOutcome O = FCricketOutcomeInterpreter::Interpret(R);
		TestEqual(TEXT("Six off the bat"), O.RunsOffBat, 6);
		TestTrue(TEXT("Marked a boundary"), O.bBoundary);
		TestEqual(TEXT("Legal"), (int32)O.Legality, (int32)ECricketDeliveryLegality::Legal);
	}
	// Taken on the full -> caught.
	{
		FCricketBallResult R; R.bStruck = true; R.bCaught = true;
		TestEqual(TEXT("Caught"), (int32)FCricketOutcomeInterpreter::Interpret(R).Dismissal, (int32)ECricketDismissal::Caught);
	}
	// Hit the stumps -> bowled.
	{
		FCricketBallResult R; R.bHitStumps = true;
		TestEqual(TEXT("Bowled"), (int32)FCricketOutcomeInterpreter::Interpret(R).Dismissal, (int32)ECricketDismissal::Bowled);
	}
	// Down the leg side for runs without contact -> byes.
	{
		FCricketBallResult R; R.bStruck = false; R.RunsRun = 2;
		const FOutcome O = FCricketOutcomeInterpreter::Interpret(R);
		TestEqual(TEXT("Byes classified"), (int32)O.ExtraType, (int32)ECricketExtraType::Bye);
		TestEqual(TEXT("Two byes"), O.RanExtraRuns, 2);
	}
	// Wide that also gets a batter run out.
	{
		FCricketBallResult R; R.bWide = true; R.RunsRun = 1; R.bRunOut = true; R.bRunOutStriker = false;
		const FOutcome O = FCricketOutcomeInterpreter::Interpret(R);
		TestEqual(TEXT("Wide"), (int32)O.Legality, (int32)ECricketDeliveryLegality::Wide);
		TestEqual(TEXT("Run out on the wide"), (int32)O.Dismissal, (int32)ECricketDismissal::RunOut);
		TestFalse(TEXT("Non-striker was out"), O.bDismissedStriker);
	}

	// And it feeds the engine end-to-end: a generated six scores six.
	UCricketMatchEngine* E = NewTossed();
	E->SetBowler(TEXT("AUS0"));
	FCricketBallResult Six; Six.bStruck = true; Six.bBoundarySix = true;
	E->ApplyDelivery(FCricketOutcomeInterpreter::Interpret(Six));
	TestEqual(TEXT("Engine scored the interpreted six"), E->GetActiveInnings().Totals.Runs, 6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
