#include "CricketBroadcastDirector.h"

ECricketBroadcastCamera FCricketBroadcastDirector::DesiredLiveCamera(
	bool bWaitingToBowl, bool bBallInFlight, bool bBallChasingRope) const
{
	// A struck ball racing to the rope -> follow it to the boundary.
	if (bBallChasingRope) { return ECricketBroadcastCamera::Boundary; }
	// The bowler running in -> the bowling angle reads the seam/release.
	if (bWaitingToBowl)   { return ECricketBroadcastCamera::Bowling; }
	// Ball in the air after a shot -> the wide broadcast frame holds the whole field.
	if (bBallInFlight)    { return ECricketBroadcastCamera::MainBroadcast; }
	// Between balls -> the iconic side-on broadcast master.
	return ECricketBroadcastCamera::MainBroadcast;
}

ECricketBroadcastCamera FCricketBroadcastDirector::SelectLiveCamera(
	bool bWaitingToBowl, bool bBallInFlight, bool bBallChasingRope, float DeltaSeconds)
{
	HeldSeconds += DeltaSeconds;
	const ECricketBroadcastCamera Desired = DesiredLiveCamera(bWaitingToBowl, bBallInFlight, bBallChasingRope);
	if (Desired != Current)
	{
		// A boundary chase is urgent and cuts immediately; everything else waits out
		// the minimum hold so the master shot doesn't flicker.
		const bool bUrgent = (Desired == ECricketBroadcastCamera::Boundary);
		if (bUrgent || HeldSeconds >= MinHoldSeconds)
		{
			Current = Desired;
			HeldSeconds = 0.0f;
		}
	}
	return Current;
}

ECricketBroadcastCamera FCricketBroadcastDirector::SelectCameraForEvent(const FCricketPresentationEvent& Event)
{
	switch (Event.Type)
	{
	case ECricketPresentationEventType::Six:
		return ECricketBroadcastCamera::Boundary;       // sail it over the rope
	case ECricketPresentationEventType::Boundary:
		return ECricketBroadcastCamera::Boundary;
	case ECricketPresentationEventType::Wicket:
		// Bowled/stumped read best from the stump cam; everything else from the master.
		return (Event.Dismissal == ECricketDismissal::Bowled || Event.Dismissal == ECricketDismissal::Stumped)
			? ECricketBroadcastCamera::Stump : ECricketBroadcastCamera::MainBroadcast;
	case ECricketPresentationEventType::Milestone:
		return ECricketBroadcastCamera::Batting;        // hold on the celebrating batter
	case ECricketPresentationEventType::MatchResult:
		return ECricketBroadcastCamera::MainBroadcast;  // pull wide for the celebrations
	default:
		return ECricketBroadcastCamera::MainBroadcast;
	}
}

ECricketCameraMode FCricketBroadcastDirector::CameraModeFor(ECricketBroadcastCamera Camera)
{
	switch (Camera)
	{
	case ECricketBroadcastCamera::MainBroadcast: return ECricketCameraMode::Spectator;
	case ECricketBroadcastCamera::Bowling:       return ECricketCameraMode::Bowling;
	case ECricketBroadcastCamera::Batting:       return ECricketCameraMode::Batting;
	case ECricketBroadcastCamera::Boundary:      return ECricketCameraMode::Fielding;   // ball-following follow cam
	case ECricketBroadcastCamera::Stump:         return ECricketCameraMode::BallFollow; // low, tight on the stumps
	case ECricketBroadcastCamera::Replay:        return ECricketCameraMode::Orbit;      // orbit the action for replays
	default:                                     return ECricketCameraMode::Spectator;
	}
}

FString FCricketBroadcastDirector::CameraName(ECricketBroadcastCamera Camera)
{
	switch (Camera)
	{
	case ECricketBroadcastCamera::MainBroadcast: return TEXT("Main Broadcast");
	case ECricketBroadcastCamera::Bowling:       return TEXT("Bowling");
	case ECricketBroadcastCamera::Batting:       return TEXT("Batting");
	case ECricketBroadcastCamera::Boundary:      return TEXT("Boundary");
	case ECricketBroadcastCamera::Stump:         return TEXT("Stump");
	case ECricketBroadcastCamera::Replay:        return TEXT("Replay");
	default:                                      return TEXT("Unknown");
	}
}
