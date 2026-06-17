#pragma once

#include "CoreMinimal.h"
#include "CricketFieldingTypes.h"
#include "CricketPhysicsTypes.h"

class FCricketBallIntegrator;

/**
 * FCricketFieldingPredictor — the reusable Ball Prediction System.
 *
 * Pure, stateless functions that turn the live ball + the fielders' capabilities
 * into the three things fielding (and later AI) needs:
 *
 *   1. PredictBall   — forward-integrate the REAL ball (via FCricketTrajectory
 *                      predictor) into landing/apex/catch facts. Bounce prediction.
 *   2. SolveIntercept— the earliest point a given fielder can meet that path, and
 *                      whether it is a catch (in the air) or a ground field, with a
 *                      difficulty that falls out of the geometry. Catch & intercept
 *                      prediction.
 *   3. SolveThrow    — a ballistic aim from A to B at a release speed (throw to
 *                      stumps / keeper / fielder; the run-out direct hit).
 *
 * None of this drives the simulation; it only reads predicted physics so fielders
 * can REACT to it.
 */
class CRICKETPHYSICS_API FCricketFieldingPredictor
{
public:
	/** Forecast the live ball. Integrator is taken by value (caller's is untouched). */
	static FCricketBallPrediction PredictBall(
		const FCricketBallState& BallState,
		FCricketBallIntegrator Integrator,
		const FCricketPredictionParams& Params);

	/** Earliest reachable meeting point of Query's fielder against Prediction. */
	static FCricketInterceptResult SolveIntercept(
		const FCricketBallPrediction& Prediction,
		const FCricketInterceptQuery& Query);

	/**
	 * Ballistic aim from FromM to TargetM at LaunchSpeedMS under gravity. bPreferFlat
	 * picks the flatter (faster, lower) of the two arcs — what you want for a run-out;
	 * false lobs it. Infeasible if the target is out of range for the speed.
	 */
	static FCricketThrowSolution SolveThrow(
		const FVector& FromM,
		const FVector& TargetM,
		double LaunchSpeedMS,
		bool bPreferFlat = true);
};
