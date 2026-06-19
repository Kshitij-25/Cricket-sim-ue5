#include "CricketMatchSnapshot.h"
#include "CricketMatchEngine.h"

FCricketMatchSnapshot FCricketMatchSnapshot::Capture(const UCricketMatchEngine& Engine)
{
	FCricketMatchSnapshot S;
	S.bValid = true;
	S.State = Engine.GetMatchState();
	S.bResultDecided = Engine.GetResult().bDecided;
	S.bTie = Engine.GetResult().bTie;
	S.WinningTeam = Engine.GetResult().WinningTeam;
	S.ResultSummary = Engine.GetResult().Summary;

	// Only an in-progress innings has a live scorecard to mirror.
	if (Engine.GetActiveInningsIndex() != INDEX_NONE)
	{
		const FCricketInningsScorecard& Card = Engine.GetActiveInnings();
		S.BattingTeam = Card.BattingTeam;
		S.BowlingTeam = Card.BowlingTeam;
		S.TeamRuns = Card.Totals.Runs;
		S.TeamWickets = Card.Totals.Wickets;
		S.LegalBalls = Card.Totals.LegalBalls;

		if (Card.Batters.IsValidIndex(Card.StrikerIndex))
		{
			const FCricketBatterStats& Striker = Card.Batters[Card.StrikerIndex];
			S.StrikerName = Striker.Name;
			S.StrikerRuns = Striker.Runs;
			S.StrikerBalls = Striker.Balls;
		}
		if (Card.Bowlers.IsValidIndex(Card.CurrentBowler))
		{
			const FCricketBowlerStats& Bowler = Card.Bowlers[Card.CurrentBowler];
			S.BowlerName = Bowler.Name;
			S.BowlerWickets = Bowler.Wickets;
		}
	}

	S.bChasing = (S.State == ECricketMatchState::SecondInnings);
	S.Target = Engine.GetTarget();
	S.RunsRequired = Engine.RunsRequired();
	S.BallsRemaining = Engine.BallsRemaining();
	S.RequiredRunRate = Engine.RequiredRunRate();
	return S;
}
