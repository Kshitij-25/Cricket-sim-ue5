#include "CricketScorePresentationModel.h"

bool FCricketScorePresentationModel::OnBall(const FCricketMatchSnapshot& Before, const FCricketMatchSnapshot& After)
{
	if (!After.bValid) { return false; }

	// Runs added this ball (team total moves by runs off bat + extras).
	const int32 RunsThisBall = FMath::Max(0, After.TeamRuns - Before.TeamRuns);
	const bool bWicketThisBall = After.TeamWickets > Before.TeamWickets;
	const bool bLegalBall = After.LegalBalls > Before.LegalBalls;

	// --- Partnership: grows with the stand, resets when a wicket falls --------
	if (bWicketThisBall)
	{
		// The runs/legal ball still count toward the broken stand before we reset.
		PartnershipRuns += RunsThisBall;
		if (bLegalBall) { ++PartnershipBalls; }
		// New pair at the crease.
		PartnershipRuns = 0;
		PartnershipBalls = 0;
	}
	else
	{
		PartnershipRuns += RunsThisBall;
		if (bLegalBall) { ++PartnershipBalls; }
	}

	// --- Over accumulator -----------------------------------------------------
	OverRuns += RunsThisBall;
	if (bWicketThisBall) { ++OverWickets; }

	bHasOverSummary = false;
	const bool bOverComplete = bLegalBall && (After.LegalBalls % 6 == 0) && After.LegalBalls > 0;
	if (bOverComplete)
	{
		const int32 OverNumber = After.LegalBalls / 6;
		LastOverSummary = FString::Printf(TEXT("Over %d: %d run%s, %d wkt%s — %d/%d"),
			OverNumber,
			OverRuns, OverRuns == 1 ? TEXT("") : TEXT("s"),
			OverWickets, OverWickets == 1 ? TEXT("") : TEXT("s"),
			After.TeamRuns, After.TeamWickets);
		bHasOverSummary = true;
		OverRuns = 0;
		OverWickets = 0;
	}

	return bHasOverSummary;
}

FString FCricketScorePresentationModel::PartnershipText() const
{
	return FString::Printf(TEXT("Partnership %d (%d)"), PartnershipRuns, PartnershipBalls);
}

FString FCricketScorePresentationModel::ChaseText(const FCricketMatchSnapshot& Snapshot)
{
	if (!Snapshot.bChasing || Snapshot.RunsRequired <= 0 || Snapshot.BallsRemaining <= 0)
	{
		return FString();
	}
	return FString::Printf(TEXT("Need %d from %d • RRR %.2f"),
		Snapshot.RunsRequired, Snapshot.BallsRemaining, Snapshot.RequiredRunRate);
}

FString FCricketScorePresentationModel::MilestoneText(const FCricketPresentationEvent& Event, const FCricketMatchSnapshot& After)
{
	if (Event.Type != ECricketPresentationEventType::Milestone) { return FString(); }

	switch (Event.Milestone)
	{
	case ECricketMilestoneType::BatterFifty:
	case ECricketMilestoneType::BatterCentury:
	{
		const TCHAR* Label = (Event.Milestone == ECricketMilestoneType::BatterCentury) ? TEXT("CENTURY") : TEXT("FIFTY");
		// Balls faced is in the post-ball snapshot for the striker who reached it.
		if (After.StrikerName == Event.PrimaryPlayer && After.StrikerBalls > 0)
		{
			return FString::Printf(TEXT("%s • %s (%d balls)"), Label, *Event.PrimaryPlayer, After.StrikerBalls);
		}
		return FString::Printf(TEXT("%s • %s"), Label, *Event.PrimaryPlayer);
	}
	case ECricketMilestoneType::BowlerThreeFor:
		return FString::Printf(TEXT("THREE WICKETS • %s"), *Event.PrimaryPlayer);
	case ECricketMilestoneType::BowlerFiveFor:
		return FString::Printf(TEXT("FIVE-WICKET HAUL • %s"), *Event.PrimaryPlayer);
	default:
		// Team milestones already carry a finished headline.
		return Event.Headline;
	}
}
