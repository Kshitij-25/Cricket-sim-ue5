#pragma once

#include "CoreMinimal.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketFieldingTypes.generated.h"

/**
 * CricketFieldingTypes — data models for the fielding layer's REASONING, kept in
 * the pure physics module so the same prediction/intercept/throw maths is reused
 * by the gameplay fielder component now and by AI later. Everything is SI (metres,
 * m/s, seconds, radians) in the shared world axes (see CricketPhysicsConstants.h).
 *
 * Nothing here scripts an outcome: a prediction is a forward integration of the
 * REAL ball through the REAL model (FCricketTrajectoryPredictor), and an intercept
 * is a reachability test of a fielder against that predicted path.
 */

/** What kind of play an intercept represents. */
UENUM(BlueprintType)
enum class ECricketInterceptKind : uint8
{
	None        UMETA(DisplayName = "None"),         // unreachable (e.g. a boundary)
	Catch       UMETA(DisplayName = "Catch"),        // taken in the air before it bounces
	GroundField UMETA(DisplayName = "Ground Field")  // fielded off the ground after a bounce
};

/** How hard a reachable chance is — derived from time-slack and how far the fielder must stretch. */
UENUM(BlueprintType)
enum class ECricketCatchDifficulty : uint8
{
	Regulation UMETA(DisplayName = "Regulation"), // comfortably under it with time to spare
	Running    UMETA(DisplayName = "Running"),    // reached on the move, little slack
	Diving     UMETA(DisplayName = "Diving"),     // only via the dive/reach radius
	Impossible UMETA(DisplayName = "Impossible")  // cannot be reached in time
};

/**
 * FCricketBallPrediction — a physics-true forecast of the live ball: the sampled
 * path plus the headline facts fielders reason about (where/when it lands, its
 * apex, total flight). Built from FCricketTrajectoryPredictor, never a scripted curve.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBallPrediction
{
	GENERATED_BODY()

	/** True once a prediction has been computed from a live ball. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") bool bValid = false;

	/** The sampled path (m, time) — shared format with the trajectory predictor. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") TArray<FCricketTrajectorySample> Path;

	/** First predicted ground contact (m) and when (s). bWillBounce false => never lands in window. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") bool bWillBounce = false;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") FVector LandingPointM = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double TimeToLandSec = 0.0;

	/** Highest point of the flight (m), its height and when. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") FVector ApexM = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double ApexHeightM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double TimeToApexSec = 0.0;

	/** Time of the last predicted sample (s). */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double FlightTimeSec = 0.0;

	/** Time of the first in-air -> ground transition used to split catch vs ground field (s). */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double FirstBounceTimeSec = 0.0;

	/** Interpolated ball position (m) at time T (s) along the predicted path. */
	FVector PositionAtTime(double T) const;
	/** Interpolated ball velocity (m/s) at time T (s) along the predicted path. */
	FVector VelocityAtTime(double T) const;
};

/**
 * FCricketInterceptQuery — one fielder's physical capabilities, posed against a
 * prediction. These are CAPABILITIES (run speed, reaction, reach), not a result.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketInterceptQuery
{
	GENERATED_BODY()

	/** Fielder's current position (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") FVector FielderPosM = FVector::ZeroVector;

	/** Top running speed (m/s). Typical 6–8 m/s for a sprinting fielder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") double MaxSpeedMS = 7.0;

	/** Reaction delay before the fielder starts moving (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") double ReactionTimeSec = 0.22;

	/** Lateral reach at the end of the run — the dive/outstretch radius (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") double ReachRadiusM = 1.6;

	/** Highest catchable height (m): standing reach + jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") double CatchReachHeightM = 2.6;

	/** Lowest catchable height (m): a low/diving catch near the turf. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") double CatchReachLowM = 0.1;

	/** Ball must be at/under this height (m) after the bounce to be fielded cleanly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding") double GroundFieldHeightM = 0.7;
};

/**
 * FCricketInterceptResult — the earliest point a fielder can meet the predicted
 * ball, with the difficulty that follows from the geometry. Purely derived.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketInterceptResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fielding") bool bCanIntercept = false;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") ECricketInterceptKind Kind = ECricketInterceptKind::None;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") ECricketCatchDifficulty Difficulty = ECricketCatchDifficulty::Impossible;

	/** Where and when the fielder meets the ball. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") FVector PointM = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double TimeSec = 0.0;

	/** Spare time after accounting for reaction + the run at top speed (s). >=0 = reachable. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double SlackSec = 0.0;

	/** Speed the fielder would need to sustain to make it (m/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double RequiredSpeedMS = 0.0;

	/** Horizontal distance from the fielder to the meeting point (m). */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double DistanceM = 0.0;
};

/**
 * FCricketThrowSolution — a ballistic aim from the fielder to a target (stumps,
 * keeper, another fielder). Solved under gravity for a given release speed; the
 * actual thrown ball is then flown by the full model, so small drag is absorbed by
 * the hit tolerance rather than scripted away.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketThrowSolution
{
	GENERATED_BODY()

	/** False if the target is out of range for the release speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") bool bFeasible = false;

	/** Release velocity (m/s) that lands the throw on the target. */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") FVector LaunchVelocityMS = FVector::ZeroVector;

	/** Time of flight to the target (s). */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double FlightTimeSec = 0.0;

	/** Launch elevation above horizontal (deg). */
	UPROPERTY(BlueprintReadOnly, Category = "Fielding") double LaunchElevationDeg = 0.0;
};
