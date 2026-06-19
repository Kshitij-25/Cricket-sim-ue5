#include "CricketEventClassifier.h"
#include "CricketScoringTypes.h"   // FCricketDeliveryOutcome, ECricketDismissal

namespace
{
	FString PresoDismissalText(ECricketDismissal D)
	{
		switch (D)
		{
		case ECricketDismissal::Bowled:  return TEXT("bowled");
		case ECricketDismissal::Caught:  return TEXT("caught");
		case ECricketDismissal::LBW:     return TEXT("lbw");
		case ECricketDismissal::RunOut:  return TEXT("run out");
		case ECricketDismissal::Stumped: return TEXT("stumped");
		default:                         return TEXT("out");
		}
	}

	// Did the striker's score cross `Mark` on this ball?
	bool CrossedMark(int32 BeforeVal, int32 AfterVal, int32 Mark)
	{
		return BeforeVal < Mark && AfterVal >= Mark;
	}
}

float FCricketEventClassifier::CrowdImpulseFor(ECricketPresentationEventType Type, ECricketPresentationSeverity Severity)
{
	// Base impulse by type, scaled up for the higher-severity (chase-pressure) variants.
	float Base = 0.0f;
	switch (Type)
	{
	case ECricketPresentationEventType::Six:         Base = 0.60f; break;
	case ECricketPresentationEventType::Boundary:    Base = 0.40f; break;
	case ECricketPresentationEventType::Wicket:      Base = 0.70f; break;
	case ECricketPresentationEventType::Milestone:   Base = 0.45f; break;
	case ECricketPresentationEventType::MatchResult: Base = 1.00f; break;
	default:                                         Base = 0.0f;  break;
	}
	const float SevScale =
		Severity == ECricketPresentationSeverity::Defining ? 1.0f :
		Severity == ECricketPresentationSeverity::High     ? 0.9f :
		Severity == ECricketPresentationSeverity::Medium   ? 0.75f : 0.6f;
	return FMath::Clamp(Base * (0.7f + 0.3f * (SevScale / 1.0f)) + (SevScale - 0.6f) * 0.4f, 0.0f, 1.0f);
}

FCricketPresentationEvent FCricketEventClassifier::ClassifyBoundary(
	const FCricketDeliveryOutcome& Outcome, const FCricketMatchSnapshot& After)
{
	FCricketPresentationEvent E;
	if (!Outcome.bBoundary) { return E; }

	const bool bSix = (Outcome.RunsOffBat >= 6);
	E.Type = bSix ? ECricketPresentationEventType::Six : ECricketPresentationEventType::Boundary;
	// In a tight chase a boundary matters more; otherwise a six > four.
	const bool bPressure = After.bChasing && After.BallsRemaining <= 36 && After.RunsRequired <= 60 && After.RunsRequired > 0;
	E.Severity = bSix ? (bPressure ? ECricketPresentationSeverity::Defining : ECricketPresentationSeverity::High)
	                  : (bPressure ? ECricketPresentationSeverity::High : ECricketPresentationSeverity::Medium);
	E.PrimaryPlayer = After.StrikerName;
	E.Headline = bSix ? TEXT("SIX!") : TEXT("FOUR!");
	E.bReplayCandidate = true;
	E.CrowdImpulse = CrowdImpulseFor(E.Type, E.Severity);
	return E;
}

FCricketPresentationEvent FCricketEventClassifier::ClassifyWicket(
	const FCricketDeliveryOutcome& Outcome, const FCricketMatchSnapshot& Before, const FCricketMatchSnapshot& After)
{
	FCricketPresentationEvent E;
	if (Outcome.Dismissal == ECricketDismissal::NotOut) { return E; }

	E.Type = ECricketPresentationEventType::Wicket;
	E.Dismissal = Outcome.Dismissal;
	// The batter who was at the crease before the ball is the one who fell.
	E.PrimaryPlayer = Before.StrikerName;
	E.SecondaryPlayer = After.BowlerName.IsEmpty() ? Before.BowlerName : After.BowlerName;
	E.Headline = FString::Printf(TEXT("WICKET — %s"), *PresoDismissalText(Outcome.Dismissal));

	// A top-order strike or a strike that breaks a strong chase reads bigger.
	const bool bLateChase = After.bChasing && After.BallsRemaining <= 36;
	E.Severity = bLateChase ? ECricketPresentationSeverity::Defining : ECricketPresentationSeverity::High;
	E.bReplayCandidate = true;
	E.CrowdImpulse = CrowdImpulseFor(E.Type, E.Severity);
	return E;
}

FCricketPresentationEvent FCricketEventClassifier::ClassifyMilestone(
	const FCricketMatchSnapshot& Before, const FCricketMatchSnapshot& After)
{
	FCricketPresentationEvent E;

	// Batter milestones — only credit the striker who actually advanced this ball.
	const bool bSameStriker = !After.StrikerName.IsEmpty() && After.StrikerName == Before.StrikerName;
	if (bSameStriker && CrossedMark(Before.StrikerRuns, After.StrikerRuns, 100))
	{
		E.Type = ECricketPresentationEventType::Milestone;
		E.Milestone = ECricketMilestoneType::BatterCentury;
		E.PrimaryPlayer = After.StrikerName;
		E.Severity = ECricketPresentationSeverity::High;
		E.Headline = FString::Printf(TEXT("CENTURY • %s"), *After.StrikerName);
	}
	else if (bSameStriker && CrossedMark(Before.StrikerRuns, After.StrikerRuns, 50))
	{
		E.Type = ECricketPresentationEventType::Milestone;
		E.Milestone = ECricketMilestoneType::BatterFifty;
		E.PrimaryPlayer = After.StrikerName;
		E.Severity = ECricketPresentationSeverity::Medium;
		E.Headline = FString::Printf(TEXT("FIFTY • %s"), *After.StrikerName);
	}
	// Bowler milestones — same bowler reaching a wicket landmark.
	else if (After.BowlerName == Before.BowlerName && !After.BowlerName.IsEmpty()
		&& CrossedMark(Before.BowlerWickets, After.BowlerWickets, 5))
	{
		E.Type = ECricketPresentationEventType::Milestone;
		E.Milestone = ECricketMilestoneType::BowlerFiveFor;
		E.PrimaryPlayer = After.BowlerName;
		E.Severity = ECricketPresentationSeverity::High;
		E.Headline = FString::Printf(TEXT("FIVE-FOR • %s"), *After.BowlerName);
	}
	else if (After.BowlerName == Before.BowlerName && !After.BowlerName.IsEmpty()
		&& CrossedMark(Before.BowlerWickets, After.BowlerWickets, 3))
	{
		E.Type = ECricketPresentationEventType::Milestone;
		E.Milestone = ECricketMilestoneType::BowlerThreeFor;
		E.PrimaryPlayer = After.BowlerName;
		E.Severity = ECricketPresentationSeverity::Medium;
		E.Headline = FString::Printf(TEXT("THREE-FOR • %s"), *After.BowlerName);
	}
	// Team milestones (only when no individual milestone took the beat).
	else
	{
		struct FTeamMark { int32 Runs; ECricketMilestoneType Type; const TCHAR* Label; };
		static const FTeamMark Marks[] = {
			{ 200, ECricketMilestoneType::TeamTwoHundred,   TEXT("200 UP") },
			{ 150, ECricketMilestoneType::TeamHundredFifty, TEXT("150 UP") },
			{ 100, ECricketMilestoneType::TeamHundred,      TEXT("100 UP") },
			{ 50,  ECricketMilestoneType::TeamFifty,        TEXT("50 UP") },
		};
		for (const FTeamMark& M : Marks)
		{
			if (CrossedMark(Before.TeamRuns, After.TeamRuns, M.Runs))
			{
				E.Type = ECricketPresentationEventType::Milestone;
				E.Milestone = M.Type;
				E.PrimaryPlayer = After.BattingTeam;
				E.Severity = ECricketPresentationSeverity::Medium;
				E.Headline = FString::Printf(TEXT("%s • %s"), M.Label, *After.BattingTeam);
				break;
			}
		}
	}

	if (E.IsValid())
	{
		E.bReplayCandidate = false; // milestones get a graphic, not a ball replay
		E.CrowdImpulse = CrowdImpulseFor(E.Type, E.Severity);
	}
	return E;
}

FCricketPresentationEvent FCricketEventClassifier::ClassifyResult(const FCricketMatchSnapshot& After)
{
	FCricketPresentationEvent E;
	if (!After.bResultDecided) { return E; }

	E.Type = ECricketPresentationEventType::MatchResult;
	E.Severity = ECricketPresentationSeverity::Defining;
	E.PrimaryPlayer = After.bTie ? FString() : After.WinningTeam;
	E.Headline = After.ResultSummary.IsEmpty()
		? (After.bTie ? TEXT("MATCH TIED") : TEXT("MATCH WON"))
		: After.ResultSummary;
	E.bReplayCandidate = true;
	E.bMatchDefining = true;
	E.CrowdImpulse = 1.0f;
	return E;
}

TArray<FCricketPresentationEvent> FCricketEventClassifier::ClassifyDelivery(
	const FCricketDeliveryOutcome& Outcome,
	const FCricketMatchSnapshot& Before,
	const FCricketMatchSnapshot& After)
{
	TArray<FCricketPresentationEvent> Events;

	// 1) Wicket is the highest-priority single-ball beat.
	const FCricketPresentationEvent Wicket = ClassifyWicket(Outcome, Before, After);
	if (Wicket.IsValid()) { Events.Add(Wicket); }

	// 2) A boundary/six (mutually exclusive with a wicket on the same legal ball, but
	//    a no-ball can be hit for a boundary — keep both if present).
	const FCricketPresentationEvent Boundary = ClassifyBoundary(Outcome, After);
	if (Boundary.IsValid()) { Events.Add(Boundary); }

	// 3) Any milestone crossed by the runs from this ball.
	const FCricketPresentationEvent Milestone = ClassifyMilestone(Before, After);
	if (Milestone.IsValid()) { Events.Add(Milestone); }

	// 4) The match-deciding moment, if this ball settled the match.
	if (After.bResultDecided && !Before.bResultDecided)
	{
		FCricketPresentationEvent Result = ClassifyResult(After);
		// If a boundary/six/wicket sealed it, mark that beat as defining too.
		if (Events.Num() > 0)
		{
			Events[0].bMatchDefining = true;
			Events[0].Severity = ECricketPresentationSeverity::Defining;
			Events[0].CrowdImpulse = 1.0f;
		}
		Events.Add(Result);
	}

	return Events;
}
