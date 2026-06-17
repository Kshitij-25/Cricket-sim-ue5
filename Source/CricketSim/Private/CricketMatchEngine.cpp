#include "CricketMatchEngine.h"

namespace
{
	const FCricketInningsScorecard& EmptyCard()
	{
		static const FCricketInningsScorecard Empty;
		return Empty;
	}
}

// ============================ Setup & state machine ==========================

void UCricketMatchEngine::ConfigureMatch(const FCricketMatchRules& InRules,
	const FCricketSquad& TeamA, const FCricketSquad& TeamB)
{
	Rules = InRules;
	Squads[0] = TeamA;
	Squads[1] = TeamB;
	Innings[0] = FCricketInningsScorecard();
	Innings[1] = FCricketInningsScorecard();
	ActiveInnings = INDEX_NONE;
	BattingFirst = 0;
	Target = 0;
	Result = FCricketMatchResult();
	PrevOverBowlerName.Reset();
	bAwaitingBowler = false;
	SetMatchState(ECricketMatchState::PreMatch);
}

void UCricketMatchEngine::StartMatch()
{
	if (MatchState != ECricketMatchState::PreMatch) { return; }
	SetMatchState(ECricketMatchState::Toss);
}

void UCricketMatchEngine::PerformToss(int32 TossWinner, bool bWinnerBatsFirst)
{
	if (MatchState != ECricketMatchState::Toss) { return; }
	const int32 Winner = FMath::Clamp(TossWinner, 0, 1);
	BattingFirst = bWinnerBatsFirst ? Winner : (1 - Winner);

	SetMatchState(ECricketMatchState::FirstInnings);
	ActiveInnings = 0;
	Innings_Begin(0, Squads[BattingFirst], Squads[1 - BattingFirst]);
}

void UCricketMatchEngine::StartSecondInnings()
{
	if (MatchState != ECricketMatchState::InningsBreak) { return; }
	SetMatchState(ECricketMatchState::SecondInnings);
	ActiveInnings = 1;
	Innings_Begin(1, Squads[1 - BattingFirst], Squads[BattingFirst]);
}

void UCricketMatchEngine::SetMatchState(ECricketMatchState NewState)
{
	if (MatchState == NewState) { return; }
	MatchState = NewState;
	OnMatchStateChanged.Broadcast(NewState);
}

// ============================== Innings Manager ==============================

void UCricketMatchEngine::Innings_Begin(int32 InningsIndex,
	const FCricketSquad& BattingSquad, const FCricketSquad& BowlingSquad)
{
	FCricketInningsScorecard Card;
	Card.BattingTeam = BattingSquad.TeamName;
	Card.BowlingTeam = BowlingSquad.TeamName;
	Card.Batters.Reset();
	for (const FString& Name : BattingSquad.PlayerNames)
	{
		FCricketBatterStats B; B.Name = Name; Card.Batters.Add(B);
	}
	Card.StrikerIndex = 0;
	Card.NonStrikerIndex = 1;
	Card.NextBatterIndex = 2;
	Card.CurrentBowler = INDEX_NONE;
	if (Card.Batters.IsValidIndex(0)) { Card.Batters[0].bHasBatted = true; }
	if (Card.Batters.IsValidIndex(1)) { Card.Batters[1].bHasBatted = true; }

	Innings[InningsIndex] = Card;
	PrevOverBowlerName.Reset();
	bAwaitingBowler = true;
}

bool UCricketMatchEngine::Innings_CheckEnd()
{
	const FCricketInningsScorecard& A = Active();
	if (A.bAllOut) { return true; }
	if (A.Totals.LegalBalls >= Rules.OversPerInnings * Rules.BallsPerOver) { return true; }
	if (ActiveInnings == 1 && A.Totals.Runs >= Target) { return true; }
	return false;
}

void UCricketMatchEngine::Innings_Close()
{
	Active().bClosed = true;

	if (MatchState == ECricketMatchState::FirstInnings)
	{
		Target = Active().Totals.Runs + 1; // chase needs to PASS the first-innings total
		SetMatchState(ECricketMatchState::InningsBreak);
		return;
	}

	// Second innings closed -> decide the match.
	const int32 First = Innings[0].Totals.Runs;
	const int32 Second = Innings[1].Totals.Runs;
	Result = FCricketMatchResult();
	Result.bDecided = true;

	if (Second >= Target) // chasers passed it
	{
		const int32 WktsLeft = (PlayersPerTeam() - 1) - Innings[1].Totals.Wickets;
		Result.WinningTeam = Innings[1].BattingTeam;
		Result.Summary = FString::Printf(TEXT("%s won by %d wicket%s"),
			*Result.WinningTeam, WktsLeft, WktsLeft == 1 ? TEXT("") : TEXT("s"));
	}
	else if (Second == Target - 1) // level
	{
		Result.bTie = true;
		Result.Summary = TEXT("Match tied");
	}
	else // defended
	{
		const int32 Margin = First - Second;
		Result.WinningTeam = Innings[0].BattingTeam;
		Result.Summary = FString::Printf(TEXT("%s won by %d run%s"),
			*Result.WinningTeam, Margin, Margin == 1 ? TEXT("") : TEXT("s"));
	}
	SetMatchState(ECricketMatchState::MatchComplete);
}

// ============================== Over Manager =================================

bool UCricketMatchEngine::CanBowl(const FString& BowlerName) const
{
	if (!IsLive()) { return false; }
	if (BowlerName.IsEmpty()) { return false; }
	if (BowlerName == PrevOverBowlerName) { return false; } // no consecutive overs

	// Must be in the bowling squad.
	const FCricketInningsScorecard& A = GetActiveInnings();
	const int32 BowlSquad = (A.BowlingTeam == Squads[0].TeamName) ? 0 : 1;
	if (!Squads[BowlSquad].PlayerNames.Contains(BowlerName)) { return false; }

	// Must be under the per-bowler over cap.
	for (const FCricketBowlerStats& B : A.Bowlers)
	{
		if (B.Name == BowlerName)
		{
			return B.CompletedOvers() < Rules.MaxOversPerBowler;
		}
	}
	return true; // hasn't bowled yet
}

bool UCricketMatchEngine::SetBowler(const FString& BowlerName)
{
	if (!IsLive() || !CanBowl(BowlerName)) { return false; }
	Active().CurrentBowler = FindOrAddBowler(BowlerName);
	bAwaitingBowler = false;
	return true;
}

void UCricketMatchEngine::Over_Advance(bool bLegal, bool /*bWicketFell*/)
{
	FCricketInningsScorecard& A = Active();
	if (!bLegal) { return; } // illegal deliveries don't advance the over

	if (A.Totals.LegalBalls > 0 && (A.Totals.LegalBalls % Rules.BallsPerOver) == 0)
	{
		// The over is complete.
		FCricketBowlerStats& BW = CurrentBowlerStats();
		if (BW.RunsThisOver == 0) { BW.Maidens++; } // no runs charged to the bowler
		BW.RunsThisOver = 0;
		BW.BallsThisOver = 0;
		PrevOverBowlerName = BW.Name;

		A.CurrentBowler = INDEX_NONE;
		bAwaitingBowler = true;
		SwapStrike(); // batsmen change ends between overs
		OnOverComplete.Broadcast();
	}
}

// =============================== Score Engine ================================

void UCricketMatchEngine::Score_Apply(const FCricketDeliveryOutcome& O,
	int32& OutBowlerRuns, int32& OutRanRuns, bool bLegal)
{
	FCricketInningsScorecard& A = Active();
	FCricketBatterStats& Striker = A.Batters[A.StrikerIndex];
	FCricketBowlerStats& Bowler = CurrentBowlerStats();

	const int32 Penalty = (O.Legality == ECricketDeliveryLegality::Wide || O.Legality == ECricketDeliveryLegality::NoBall) ? 1 : 0;
	const int32 BatRuns = (O.Legality == ECricketDeliveryLegality::Wide) ? 0 : O.RunsOffBat;

	// Extras: wides & no-balls (penalty + anything run on them), and byes/leg-byes.
	int32 Extras = 0;
	if (O.Legality == ECricketDeliveryLegality::Wide || O.Legality == ECricketDeliveryLegality::NoBall)
	{
		Extras += Penalty + O.RanExtraRuns;
	}
	else if (O.ExtraType != ECricketExtraType::None)
	{
		Extras += O.RanExtraRuns;
	}

	const int32 TeamRuns = Penalty + BatRuns + O.RanExtraRuns;
	A.Totals.Runs += TeamRuns;
	A.Totals.Extras += Extras;

	// Striker credit (never on a wide; byes/leg-byes are not the striker's runs).
	if (O.Legality != ECricketDeliveryLegality::Wide)
	{
		Striker.Runs += BatRuns;
		if (O.bBoundary && BatRuns == 4) { Striker.Fours++; }
		if (O.bBoundary && BatRuns == 6) { Striker.Sixes++; }
	}

	// A ball is "faced"/"bowled" only when it is legal.
	if (bLegal)
	{
		Striker.Balls++;
		A.Totals.LegalBalls++;
		Bowler.LegalBalls++;
		Bowler.BallsThisOver++;
	}

	// The bowler is charged with the penalty + runs off the bat (NOT byes/leg-byes).
	OutBowlerRuns = Penalty + BatRuns;
	Bowler.RunsConceded += OutBowlerRuns;
	Bowler.RunsThisOver += OutBowlerRuns;

	// Runs physically RUN (drive the strike), excluding boundaries and the penalty.
	if (O.Legality == ECricketDeliveryLegality::Wide)
	{
		OutRanRuns = O.RanExtraRuns;
	}
	else if (O.Legality == ECricketDeliveryLegality::NoBall)
	{
		OutRanRuns = (O.bBoundary ? 0 : O.RunsOffBat) + O.RanExtraRuns;
	}
	else
	{
		OutRanRuns = (O.ExtraType != ECricketExtraType::None) ? O.RanExtraRuns : (O.bBoundary ? 0 : O.RunsOffBat);
	}
}

// ============================== Wicket Manager ===============================

void UCricketMatchEngine::Wicket_Apply(const FCricketDeliveryOutcome& O)
{
	FCricketInningsScorecard& A = Active();

	// Positions here are AFTER the running crossing has been applied.
	const int32 OutIdx = (O.Dismissal == ECricketDismissal::RunOut)
		? (O.bDismissedStriker ? A.StrikerIndex : A.NonStrikerIndex)
		: A.StrikerIndex; // the batter facing is the one out for bowled/caught/lbw/stumped

	FCricketBatterStats& Bat = A.Batters[OutIdx];
	Bat.bOut = true;
	Bat.Dismissal = O.Dismissal;

	const bool bBowlerCredit =
		O.Dismissal == ECricketDismissal::Bowled || O.Dismissal == ECricketDismissal::Caught ||
		O.Dismissal == ECricketDismissal::LBW    || O.Dismissal == ECricketDismissal::Stumped;
	if (bBowlerCredit)
	{
		FCricketBowlerStats& BW = CurrentBowlerStats();
		Bat.BowlerName = BW.Name;
		BW.Wickets++;
	}

	A.Totals.Wickets++;
	OnWicket.Broadcast(O.Dismissal);

	// All out at (PlayersPerTeam - 1) wickets; otherwise the next batter walks in
	// at the dismissed batter's end.
	if (A.Totals.Wickets >= PlayersPerTeam() - 1)
	{
		A.bAllOut = true;
		return;
	}
	const int32 NewIdx = A.NextBatterIndex++;
	if (A.Batters.IsValidIndex(NewIdx))
	{
		A.Batters[NewIdx].bHasBatted = true;
		if (OutIdx == A.StrikerIndex) { A.StrikerIndex = NewIdx; } else { A.NonStrikerIndex = NewIdx; }
	}
	else
	{
		A.bAllOut = true;
	}
}

// ================================ The ball ===================================

bool UCricketMatchEngine::ApplyDelivery(const FCricketDeliveryOutcome& Outcome)
{
	if (!IsLive()) { return false; }
	FCricketInningsScorecard& A = Active();
	if (A.bClosed || A.CurrentBowler == INDEX_NONE) { return false; }

	const bool bLegal = (Outcome.Legality == ECricketDeliveryLegality::Legal);

	int32 BowlerRuns = 0, RanRuns = 0;
	Score_Apply(Outcome, BowlerRuns, RanRuns, bLegal);          // Score Engine

	if (RanRuns % 2 == 1) { SwapStrike(); }                     // running crossing

	if (Outcome.Dismissal != ECricketDismissal::NotOut)        // Wicket Manager
	{
		Wicket_Apply(Outcome);
	}

	Over_Advance(bLegal, Outcome.Dismissal != ECricketDismissal::NotOut); // Over Manager

	OnBallApplied.Broadcast(Outcome);

	if (Innings_CheckEnd())                                     // Innings Manager
	{
		Innings_Close();
	}
	return true;
}

// ================================ Helpers ====================================

FCricketInningsScorecard& UCricketMatchEngine::Active()
{
	check(Innings[0].Batters.Num() >= 0); // always valid storage
	return Innings[FMath::Clamp(ActiveInnings, 0, 1)];
}

const FCricketInningsScorecard& UCricketMatchEngine::GetInnings(int32 Index) const
{
	return (Index == 0 || Index == 1) ? Innings[Index] : EmptyCard();
}

const FCricketInningsScorecard& UCricketMatchEngine::GetActiveInnings() const
{
	return (ActiveInnings == 0 || ActiveInnings == 1) ? Innings[ActiveInnings] : EmptyCard();
}

FCricketBowlerStats& UCricketMatchEngine::CurrentBowlerStats()
{
	FCricketInningsScorecard& A = Active();
	return A.Bowlers[A.CurrentBowler];
}

int32 UCricketMatchEngine::FindOrAddBowler(const FString& Name)
{
	FCricketInningsScorecard& A = Active();
	for (int32 i = 0; i < A.Bowlers.Num(); ++i)
	{
		if (A.Bowlers[i].Name == Name) { return i; }
	}
	FCricketBowlerStats B; B.Name = Name;
	return A.Bowlers.Add(B);
}

void UCricketMatchEngine::SwapStrike()
{
	FCricketInningsScorecard& A = Active();
	Swap(A.StrikerIndex, A.NonStrikerIndex);
}

double UCricketMatchEngine::RequiredRunRate() const
{
	if (MatchState != ECricketMatchState::SecondInnings) { return 0.0; }
	const int32 Need = FMath::Max(0, Target - GetActiveInnings().Totals.Runs);
	const int32 BallsLeft = BallsRemaining();
	return BallsLeft > 0 ? (6.0 * Need / BallsLeft) : 0.0;
}

int32 UCricketMatchEngine::RunsRequired() const
{
	return (ActiveInnings == 1) ? FMath::Max(0, Target - GetActiveInnings().Totals.Runs) : 0;
}

int32 UCricketMatchEngine::BallsRemaining() const
{
	if (!IsLive()) { return 0; }
	return FMath::Max(0, Rules.OversPerInnings * Rules.BallsPerOver - GetActiveInnings().Totals.LegalBalls);
}

FString UCricketMatchEngine::GetStrikerName() const
{
	const FCricketInningsScorecard& A = GetActiveInnings();
	return A.Batters.IsValidIndex(A.StrikerIndex) ? A.Batters[A.StrikerIndex].Name : FString();
}

FString UCricketMatchEngine::GetNonStrikerName() const
{
	const FCricketInningsScorecard& A = GetActiveInnings();
	return A.Batters.IsValidIndex(A.NonStrikerIndex) ? A.Batters[A.NonStrikerIndex].Name : FString();
}

FString UCricketMatchEngine::GetBowlerName() const
{
	const FCricketInningsScorecard& A = GetActiveInnings();
	return A.Bowlers.IsValidIndex(A.CurrentBowler) ? A.Bowlers[A.CurrentBowler].Name : FString();
}
