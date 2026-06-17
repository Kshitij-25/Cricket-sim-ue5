#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"
#include "CricketPitchInteraction.h"
#include "CricketTrajectoryPredictor.generated.h"

class FCricketBallIntegrator;

/** One sampled point along a predicted flight path. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketTrajectorySample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trajectory") double Time = 0.0;        // s since prediction start
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory") FVector Position = FVector::ZeroVector; // m
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory") FVector Velocity = FVector::ZeroVector; // m/s
};

/** Result of a forward prediction: the sampled path and where it bounced. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketTrajectoryPrediction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trajectory") TArray<FCricketTrajectorySample> Samples;
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory") TArray<FVector> BouncePoints; // m
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory") bool bReachedTimeLimit = false;
};

/** Inputs controlling a prediction run. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketPredictionParams
{
	GENERATED_BODY()

	/** Stop predicting after this many seconds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Trajectory") double MaxTime = 3.0;

	/** Store a sample every this many seconds (visual resolution). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Trajectory") double SampleInterval = 0.01;

	/** Pitch/ground plane height (m) used to detect predicted bounces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Trajectory") double PitchPlaneZ = 0.0;

	/** Maximum bounces to predict before stopping. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Trajectory") int32 MaxBounces = 2;

	/** If true, resolve predicted bounces through the pitch model (using PitchPatch). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Trajectory") bool bResolveBounces = true;

	/** Surface used when resolving predicted bounces. Variance is forced to 0 for a deterministic prediction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Trajectory") FCricketSurfacePatch PitchPatch;
};

/**
 * FCricketTrajectoryPredictor
 *
 * Integrates a copy of the ball state forward through the SAME aerodynamic +
 * pitch model the live ball uses — never a separate, scripted curve. Because it
 * shares the integrator, the predicted path matches the actual path exactly
 * (same inputs => same trajectory), which is the whole point: the debug overlay
 * and any future shot/AI logic reason about real physics, not an approximation.
 */
class CRICKETPHYSICS_API FCricketTrajectoryPredictor
{
public:
	/**
	 * Predict forward from InitialState. The Integrator is taken by value so the
	 * caller's live integrator (and its accumulator) is untouched.
	 */
	static FCricketTrajectoryPrediction Predict(
		const FCricketBallState& InitialState,
		FCricketBallIntegrator Integrator,
		const FCricketPredictionParams& Params);
};
