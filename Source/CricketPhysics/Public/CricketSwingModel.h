#pragma once

#include "CoreMinimal.h"
#include "CricketBattingTypes.h"
#include "CricketPhysicsTypes.h"
#include "CricketBatTypes.h"

/**
 * FCricketSwingModel — the Bat Motion System + Timing Evaluation System, as pure,
 * stateless, headless-testable functions (the batting counterpart to
 * FCricketShotGenerator, but resolved over TIME instead of teleported).
 *
 * Responsibilities:
 *   1. BuildProfile  — turn a shot + footwork + handedness into a kinematic
 *                      template (backlift/downswing/follow-through geometry, aim,
 *                      bat speed). Footwork shifts the contact zone and reach here.
 *   2. EvaluateBat   — sample the bat's full FCricketBatState at a given swing-clock
 *                      time: where the sweet spot is, the face frame, and the bat
 *                      velocity. This is the bat literally moving through space.
 *   3. DetectContact — geometric contact between the MOVING bat and the live ball
 *                      over one tick interval. Returns where on the blade the ball
 *                      met the face — so middle/edge/toe emerges from the swing, not
 *                      from any injected error.
 *   4. ClassifyTiming— label the contact Early/Perfect/Late from the swing-clock
 *                      time it occurred at. Read-only; never alters the outcome.
 *
 * The model owns NO outcome maths: DetectContact's FCricketContactSolution is fed
 * to FCricketBatCollision::Resolve, which decides exit speed, spin and deflection.
 */
class CRICKETPHYSICS_API FCricketSwingModel
{
public:
	/** Build the kinematic template for a stroke (footwork + handedness folded in). */
	static FCricketSwingProfile BuildProfile(
		ECricketShotType ShotType,
		ECricketFootwork Footwork,
		bool bRightHanded);

	/**
	 * Sample the bat at SwingTimeSec (0 = start of the downswing; negative = still
	 * in the backlift; > DownswingTime = follow-through). StanceOriginM is the
	 * striker's guard point in world SI. Fills OutPhase and OutBatSpeedMS.
	 */
	static FCricketBatState EvaluateBat(
		const FCricketSwingProfile& Profile,
		const FCricketBattingInput& Input,
		const FVector& StanceOriginM,
		double SwingTimeSec,
		ECricketSwingPhase& OutPhase,
		double& OutBatSpeedMS);

	/**
	 * Detect contact between the moving bat and the ball over a single interval.
	 * The ball travels Ball0 -> Ball1 (m) while the swing clock advances from
	 * SwingTimeStartSec by DeltaSec. The interval is sub-stepped (Substeps) so a
	 * fast downswing at a coarse frame rate still resolves the crossing.
	 *
	 * Returns true and fills OutSolution on the first sub-step where the ball
	 * reaches the bat face within the blade while closing. BatProfile supplies the
	 * blade bounds.
	 */
	static bool DetectContact(
		const FCricketSwingProfile& Profile,
		const FCricketBattingInput& Input,
		const FVector& StanceOriginM,
		const FCricketBatProfile& BatProfile,
		const FVector& Ball0M,
		const FVector& Ball1M,
		double SwingTimeStartSec,
		double DeltaSec,
		int32 Substeps,
		FCricketContactSolution& OutSolution);

	/** Label a contact's timing from the swing-clock time it occurred at. */
	static FCricketTimingResult ClassifyTiming(
		const FCricketSwingProfile& Profile,
		double ContactSwingTimeSec);

	/** Perfect-timing half-window (s). |error| under this is a middled "Perfect". */
	static constexpr double PerfectWindowSec = 0.022;
	/** Outer half-window (s). Beyond this on either side is Too Early / Too Late. */
	static constexpr double LooseWindowSec = 0.075;
};
