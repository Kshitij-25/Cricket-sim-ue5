#pragma once

#include "CoreMinimal.h"
#include "CricketAnimationTypes.h"
#include "CricketBattingTypes.h"

/**
 * FCricketAnimationModel — pure, stateless animation logic: the four state
 * machines and the action montages, with no UWorld/actor dependency so it is
 * headless-testable (and reusable by any character rig).
 *
 *   - Locomotion: ClassifyLocomotion turns a speed (+ how it is changing) into a
 *     gait state and blend.
 *   - Bowling:    MakeBowlingMontage builds the run-up -> stride -> release ->
 *     follow-through montage with the BallRelease notify at the right time.
 *   - Fielding:   MakeCatch/Pickup/Throw montages with their notifies.
 *   - Batting:    MapBattingPhase maps the swing model's phase to an anim state;
 *     MakeBattingMontage builds a swing montage with the BatImpact notify (for the
 *     timing tests and any anim-driven secondary motion).
 */
class CRICKETPHYSICS_API FCricketAnimationModel
{
public:
	// --- Locomotion state machine ---
	/**
	 * Classify locomotion from the planar speed (m/s), the signed turn rate
	 * (deg/s), and the along-track acceleration (m/s^2; negative = slowing). Prev is
	 * the last state, used only to make Stop/Turn sticky and avoid flicker.
	 */
	static FCricketLocomotionSample ClassifyLocomotion(
		double SpeedMS, double TurnRateDeg, double AccelMS2,
		ECricketLocomotionState Prev, const FCricketLocomotionConfig& Config);

	// --- Bowling action montage (run-up / stride / release / follow-through) ---
	static FCricketActionMontage MakeBowlingMontage(const FCricketBowlingActionTimeline& Timeline);

	// --- Fielding action montages ---
	static FCricketActionMontage MakeCatchMontage(double ReachTimeSec = 0.25, double SecureTimeSec = 0.35);
	static FCricketActionMontage MakePickupMontage(double ReachTimeSec = 0.30, double GatherTimeSec = 0.30);
	static FCricketActionMontage MakeThrowMontage(double WindupTimeSec = 0.35, double ReleaseTimeSec = 0.45, double RecoverTimeSec = 0.3);

	// --- Batting ---
	static ECricketBattingAnimState MapBattingPhase(ECricketSwingPhase Phase);
	/** A swing montage; BatImpact fires at ContactTimeSec (= the downswing duration). */
	static FCricketActionMontage MakeBattingMontage(double BackliftTimeSec, double DownswingTimeSec, double FollowThroughTimeSec);

	/**
	 * True for notifies that hand off to (or gate) a physics event — BallRelease,
	 * BatImpact, CatchAttempt, PickupContact, ThrowRelease. False for purely
	 * cosmetic notifies (FootPlant). Debug tooling uses this to highlight the
	 * moments that actually drive the simulation, vs. ones that are just dressing.
	 */
	static bool IsPhysicsHandoffNotify(ECricketAnimNotify Type);
};
