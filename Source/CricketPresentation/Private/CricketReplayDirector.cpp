#include "CricketReplayDirector.h"

FCricketReplayPlan FCricketReplayDirector::BuildPlan(const FCricketPresentationEvent& Event) const
{
	FCricketReplayPlan Plan;
	if (!Event.IsValid() || !Event.bReplayCandidate) { return Plan; }

	const bool bDefining = Event.bMatchDefining || Event.Severity == ECricketPresentationSeverity::Defining;

	switch (Event.Type)
	{
	case ECricketPresentationEventType::Wicket:
	{
		Plan.bShouldReplay = true;
		Plan.SlowMoRate = bDefining ? 0.2f : 0.3f;
		Plan.SecondsPerAngle = 1.6f;
		// Bowled/stumped: lead with the stumps; edges/lbw: lead with the master.
		if (Event.Dismissal == ECricketDismissal::Bowled || Event.Dismissal == ECricketDismissal::Stumped)
		{
			Plan.Angles = { ECricketBroadcastCamera::Stump, ECricketBroadcastCamera::MainBroadcast, ECricketBroadcastCamera::Bowling };
		}
		else
		{
			Plan.Angles = { ECricketBroadcastCamera::MainBroadcast, ECricketBroadcastCamera::Stump, ECricketBroadcastCamera::Bowling };
		}
		break;
	}
	case ECricketPresentationEventType::Six:
	{
		Plan.bShouldReplay = true;
		Plan.SlowMoRate = bDefining ? 0.3f : 0.4f;
		Plan.SecondsPerAngle = 1.5f;
		Plan.Angles = { ECricketBroadcastCamera::Boundary, ECricketBroadcastCamera::MainBroadcast };
		break;
	}
	case ECricketPresentationEventType::Boundary:
	{
		// Fours only replay when they carry weight (a pressure boundary in a chase).
		if (Event.Severity < MinBoundaryReplaySeverity) { return Plan; }
		Plan.bShouldReplay = true;
		Plan.SlowMoRate = 0.5f;
		Plan.SecondsPerAngle = 1.3f;
		Plan.Angles = { ECricketBroadcastCamera::Boundary, ECricketBroadcastCamera::MainBroadcast };
		break;
	}
	case ECricketPresentationEventType::MatchResult:
	{
		Plan.bShouldReplay = true;
		Plan.SlowMoRate = 0.25f;
		Plan.SecondsPerAngle = 2.0f;
		Plan.Angles = { ECricketBroadcastCamera::MainBroadcast, ECricketBroadcastCamera::Boundary, ECricketBroadcastCamera::Stump };
		break;
	}
	default:
		break; // milestones and flow beats don't roll a ball replay
	}

	return Plan;
}
