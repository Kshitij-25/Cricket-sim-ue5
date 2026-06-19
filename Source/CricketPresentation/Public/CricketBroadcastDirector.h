#pragma once

#include "CoreMinimal.h"
#include "CricketPresentationTypes.h"
#include "CricketCameraTypes.h"   // ECricketCameraMode

/**
 * FCricketBroadcastDirector — the pure camera-selection brain.
 *
 * It answers two questions, deterministically and without a UWorld:
 *   1) During LIVE play, which broadcast angle should be live given the ball state?
 *      (bowling cam as the bowler runs in; main broadcast through the shot; a boundary
 *      chase as the ball heads for the rope.)
 *   2) For an EVENT beat, which single angle best sells the moment?
 *
 * It also maps each broadcast angle onto the existing gameplay camera mode, because
 * the presentation layer only PICKS the angle — the live UCricketCameraDirectorComponent
 * still computes the actual pose from the subjects. The director only frames the sim;
 * it never moves anything in it.
 *
 * A tiny hysteresis (MinHoldSeconds) stops the live camera flickering between angles
 * on borderline frames.
 */
struct CRICKETPRESENTATION_API FCricketBroadcastDirector
{
	/** Don't cut the live camera more often than this (seconds). */
	float MinHoldSeconds = 0.75f;

	/** The angle currently selected. */
	ECricketBroadcastCamera Current = ECricketBroadcastCamera::MainBroadcast;

	/** Seconds the current angle has been held. */
	float HeldSeconds = 0.0f;

	/**
	 * Pick the live angle for the current ball state and advance the hold timer.
	 * bWaitingToBowl: the bowler is at the top of the mark (pre-delivery).
	 * bBallInFlight:  the struck ball is travelling (post-contact).
	 * Returns the (possibly unchanged) selected angle.
	 */
	ECricketBroadcastCamera SelectLiveCamera(bool bWaitingToBowl, bool bBallInFlight, bool bBallChasingRope, float DeltaSeconds);

	/** The single best angle to hold on for an event beat (ignores hysteresis). */
	static ECricketBroadcastCamera SelectCameraForEvent(const FCricketPresentationEvent& Event);

	/** Force the current selection (used when an event beat or replay takes over). */
	void Force(ECricketBroadcastCamera Camera) { Current = Camera; HeldSeconds = 0.0f; }

	/** Map a broadcast angle onto the live camera director's mode. */
	static ECricketCameraMode CameraModeFor(ECricketBroadcastCamera Camera);

	/** Display name for the debug overlay. */
	static FString CameraName(ECricketBroadcastCamera Camera);

private:
	ECricketBroadcastCamera DesiredLiveCamera(bool bWaitingToBowl, bool bBallInFlight, bool bBallChasingRope) const;
};
