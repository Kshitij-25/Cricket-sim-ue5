#pragma once

#include "CoreMinimal.h"
#include "CricketStadiumTypes.h"

/**
 * FCricketStadiumModel — pure stadium geometry + rules. Headless-testable, reused
 * by the gameplay Stadium Manager and (later) by captain AI.
 *
 *   - Boundary geometry: the ellipse radius in any direction; inside/outside tests.
 *   - Boundary rules: classify a recorded ball path as Four / Six / In-play, and
 *     validate a catch as taken inside the rope (out) vs over it (six).
 *   - Field positions: the world position of any named fielding position, scaled to
 *     the ground and mirrored for a left-hander; a standard default field.
 */
class CRICKETPHYSICS_API FCricketStadiumModel
{
public:
	// --- Boundary geometry ---

	/** Boundary radius (m) at AngleRad measured from the pitch axis. */
	static double BoundaryRadiusAtAngleM(const FCricketGroundDimensions& Dims, double AngleRad);

	/** Signed distance INSIDE the rope at PointM (m). Positive inside, negative outside. */
	static double SignedDistanceInsideM(const FCricketGroundDimensions& Dims, const FVector& PointM);

	static bool IsInsideBoundary(const FCricketGroundDimensions& Dims, const FVector& PointM)
	{
		return SignedDistanceInsideM(Dims, PointM) > 0.0;
	}

	/** The boundary point (m) in a given direction from the centre (Z = rope height). */
	static FVector BoundaryPointM(const FCricketGroundDimensions& Dims, const FVector& DirectionM);

	// --- Boundary rules ---

	/**
	 * Classify a recorded ball path. FirstBounceIdx is the path index of the first
	 * ground bounce (INDEX_NONE if it never bounced). Four if the ball bounced inside
	 * before crossing the rope; Six if it cleared the rope without bouncing inside;
	 * InPlay if it never crossed. OutCrossingPointM is filled when it crosses.
	 */
	static ECricketBoundaryResult ClassifyBoundary(
		const FCricketGroundDimensions& Dims, const TArray<FVector>& PathM,
		int32 FirstBounceIdx, FVector& OutCrossingPointM);

	/** A catch is good (out) only if taken inside the rope; outside the rope it is a six. */
	static ECricketBoundaryResult ValidateBoundaryCatch(const FCricketGroundDimensions& Dims, const FVector& CatchPointM)
	{
		return IsInsideBoundary(Dims, CatchPointM) ? ECricketBoundaryResult::CaughtAtBoundary : ECricketBoundaryResult::Six;
	}

	// --- Field positions ---

	/** The parametric definition (angle + depth) of a named position. */
	static FCricketFieldPositionDef DefaultPositionDef(ECricketFieldPosition Position);

	/** World position (m) of a fielding position; mirrored in Y for a left-hander. */
	static FVector FieldPositionWorldM(
		const FCricketGroundDimensions& Dims, ECricketFieldPosition Position, bool bRightHanded);

	/** A standard, balanced field (the default a captain would inherit). */
	static FCricketFieldPlacement DefaultField();
};
