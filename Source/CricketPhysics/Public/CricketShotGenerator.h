#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"
#include "CricketBatTypes.h"

/**
 * FCricketShotGenerator
 *
 * Turns a player/AI shot INTENT into deterministic bat kinematics (an
 * FCricketBatState) and a contact point — the input to FCricketBatCollision.
 * It never decides the outcome; it only positions and moves the bat. Timing and
 * line errors displace the bat relative to the ball so that a mishit emerges
 * from geometry (top-edged pull, toe-end drive, thin outside edge) rather than
 * from any random roll.
 *
 * Coordinate convention (matches the ball physics): +X toward the striker,
 * +Y off side (right-hander), +Z up. Left-handers are mirrored in Y.
 */
class CRICKETPHYSICS_API FCricketShotGenerator
{
public:
	/**
	 * Build the bat state and contact point for a shot played at the ball
	 * BallAtContact (its Position is taken as the contact point). PerfectTiming
	 * (TimingErrorSec = 0, LineErrorM = 0) strikes the sweet spot.
	 */
	static void GenerateBatState(
		const FCricketShotIntent& Intent,
		const FCricketBallState& BallAtContact,
		const FCricketBatProfile& Profile,
		FCricketBatState& OutBat,
		FVector& OutContactPointM);
};
