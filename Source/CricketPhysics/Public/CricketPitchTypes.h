#pragma once

#include "CoreMinimal.h"
#include "CricketPitchInteraction.h"
#include "CricketPitchTypes.generated.h"

/**
 * ECricketPitchType — the three shipping pitch personalities (plus a balanced
 * default and a custom passthrough). Each maps to a distinct FCricketSurfacePatch
 * via FCricketPitchMaterialLibrary, so the SAME delivery behaves noticeably
 * differently on each: a hard deck flies through, a dry deck grips and turns, a
 * green deck seams around. Selecting a type is the designer's one-click control.
 */
UENUM(BlueprintType)
enum class ECricketPitchType : uint8
{
	/** Fair, true surface — the neutral baseline (matches the struct defaults). */
	Balanced UMETA(DisplayName = "Balanced"),

	/** Hard, fast, bouncy deck (Perth-like): high, true, quick bounce. */
	Hard     UMETA(DisplayName = "Hard"),

	/** Dry, abrasive, dusty deck (subcontinent-like): grips, turns, pace off. */
	Dry      UMETA(DisplayName = "Dry"),

	/** Green, grassy, tinged-with-moisture deck (Hobart/England-like): seams. */
	Green    UMETA(DisplayName = "Green"),

	/** Author every field by hand; the library leaves the patch untouched. */
	Custom   UMETA(DisplayName = "Custom")
};

/**
 * FCricketPitchZone — a length band down the pitch with its own surface. Lets a
 * designer carve out, e.g., a worn "good length" patch (hard + abrasive) or a
 * rough footmark zone outside off stump for the spinners.
 *
 * Distance is measured in metres from the batter's stumps toward the bowler
 * (0 = batter's crease). A standard pitch is 20.12 m between creases.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketPitchZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch") double MinDistanceM = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch") double MaxDistanceM = 0.0;

	/** Surface in this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch") FCricketSurfacePatch Patch;
};

// ===========================================================================
//  FUTURE-PROOFING (DESIGN ONLY — data structures present, NOT yet applied in
//  sampling). These exist so deterioration, footmarks, day progression and
//  multi-day cricket can be layered on without reworking the pitch model. The
//  brief asks for the architecture, not the implementation, of these features.
// ===========================================================================

/**
 * FCricketFootmark — a localised rough patch stamped into the surface by a
 * bowler's follow-through (or a batter's movement). Designed to later raise
 * local Roughness/Wear/Unevenness around its centre so spinners can exploit the
 * rough outside off/leg stump. NOT yet consulted by SamplePatch.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketFootmark
{
	GENERATED_BODY()

	/** Centre, as (distance down pitch from batter's stumps, lateral offset) in m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future") FVector2D CentreM = FVector2D::ZeroVector;

	/** Radius of influence (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future", meta = (ClampMin = "0.0")) double RadiusM = 0.3;

	/** Severity [0,1] — how rough/worn the centre of the mark is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future", meta = (ClampMin = "0.0", ClampMax = "1.0")) double Severity = 0.5;
};

/**
 * FCricketPitchDayProgression — where we are in a (potentially multi-day) match.
 * Designed to drive deterioration: each day/session bakes the surface harder and
 * cracked early, then crumbles it late. ComputeWear() gives a single 0..1 dial
 * the profile already understands; richer per-property curves come later.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketPitchDayProgression
{
	GENERATED_BODY()

	/** Match day, 1-based (1..5 for a Test; 1 for a limited-overs game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future", meta = (ClampMin = "1", ClampMax = "5")) int32 Day = 1;

	/** Fraction through the current day's play [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future", meta = (ClampMin = "0.0", ClampMax = "1.0")) double SessionFraction = 0.0;

	/** Overs bowled so far (drives localised wear once footmarks are applied). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future", meta = (ClampMin = "0.0")) double OversBowled = 0.0;

	/**
	 * Cumulative global wear this state implies, in [0,1]. A simple monotonic
	 * model for now (≈ linear in elapsed days); the production curve will be
	 * authored per pitch type. Pure function — safe to call anywhere.
	 */
	double ComputeWear() const
	{
		const double Elapsed = (Day - 1) + FMath::Clamp(SessionFraction, 0.0, 1.0);
		return FMath::Clamp(Elapsed / 5.0, 0.0, 1.0);
	}
};
