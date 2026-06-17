#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"
#include "CricketBatTypes.h"

/**
 * FCricketBatCollision
 *
 * Stateless analytic resolver for a single bat-ball contact — the bat-side
 * counterpart to FCricketPitchInteraction, and built on the same impulse +
 * grip/skid model so behaviour is consistent and equally testable.
 *
 * Pipeline:
 *   1. Contact analysis: locate the impact on the bat face (along/across), map
 *      it to a region (sweet spot / edges / toe) and a deterministic EdgeFactor,
 *      and derive the location-dependent effective mass and restitution.
 *   2. Impulse response: normal impulse with restitution against the bat's
 *      effective mass (exit velocity, energy transfer); tangential Coulomb
 *      friction with the ball's rotational inertia (spin transfer, deflection).
 *   3. Outcomes: exit speed, launch angle, deflection angle, spin, energy.
 *
 * Nothing here is random. A mishit is the deterministic consequence of where and
 * how the bat met the ball.
 */
class CRICKETPHYSICS_API FCricketBatCollision
{
public:
	/**
	 * Resolve a contact. BallIn is the incoming ball; Bat is the bat kinematics;
	 * Profile the bat constants; ContactPointM the world-space point of contact
	 * (m). Writes the post-impact ball into BallOut and fills Report.
	 *
	 * Returns true if a forward (closing) contact was resolved.
	 */
	static bool Resolve(
		const FCricketBallState& BallIn,
		const FCricketBatState& Bat,
		const FCricketBatProfile& Profile,
		const FVector& ContactPointM,
		FCricketBallState& BallOut,
		FCricketBatImpactReport& Report);

	/**
	 * Contact analysis only (no impulse): classify the impact location. Useful
	 * for the debug overlay and for the resolver. Fills the contact-analysis
	 * fields of Report (region, side, offsets, edge factor, quality, eff mass,
	 * restitution).
	 */
	static void AnalyzeContact(
		const FCricketBatState& Bat,
		const FCricketBatProfile& Profile,
		const FVector& ContactPointM,
		FCricketBatImpactReport& Report);
};
