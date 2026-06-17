#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"
#include "CricketStadiumTypes.generated.h"

/**
 * CricketStadiumTypes — data models for the stadium as a SIMULATION ENVIRONMENT
 * (not a visual asset). The venue is geometry + rules: ground dimensions, where the
 * pitch sits, where the rope is, where fielders stand, and the atmospheric
 * conditions the ball flies through. All SI (metres), in the shared world axes.
 *
 * The boundary is modelled as an ellipse (straight boundary along the pitch axis,
 * square boundary perpendicular) so real grounds — longer straight than square, or
 * vice-versa — are one data change.
 */

/** Standard cricket fielding positions (relative to the striker). */
UENUM(BlueprintType)
enum class ECricketFieldPosition : uint8
{
	WicketKeeper  UMETA(DisplayName = "Wicketkeeper"),
	Slip          UMETA(DisplayName = "Slip"),
	Gully         UMETA(DisplayName = "Gully"),
	Point         UMETA(DisplayName = "Point"),
	Cover         UMETA(DisplayName = "Cover"),
	MidOff        UMETA(DisplayName = "Mid-off"),
	MidOn         UMETA(DisplayName = "Mid-on"),
	Midwicket     UMETA(DisplayName = "Midwicket"),
	SquareLeg     UMETA(DisplayName = "Square Leg"),
	FineLeg       UMETA(DisplayName = "Fine Leg"),
	ThirdMan      UMETA(DisplayName = "Third Man"),
	// Deep (boundary-riding) positions.
	LongOff        UMETA(DisplayName = "Long-off"),
	LongOn         UMETA(DisplayName = "Long-on"),
	DeepCover      UMETA(DisplayName = "Deep Cover"),
	DeepMidwicket  UMETA(DisplayName = "Deep Midwicket"),
	DeepPoint      UMETA(DisplayName = "Deep Point"),
	DeepSquareLeg  UMETA(DisplayName = "Deep Square Leg")
};

/** Outcome of a ball reaching (or not reaching) the boundary. */
UENUM(BlueprintType)
enum class ECricketBoundaryResult : uint8
{
	None             UMETA(DisplayName = "None"),
	InPlay           UMETA(DisplayName = "In Play"),          // never crossed the rope
	Four             UMETA(DisplayName = "Four"),             // crossed along the ground
	Six              UMETA(DisplayName = "Six"),              // cleared on the full
	CaughtAtBoundary UMETA(DisplayName = "Caught at Boundary") // taken cleanly inside the rope
};

/** Time of day — day/night now; the hook for floodlight/visibility logic later. */
UENUM(BlueprintType)
enum class ECricketTimeOfDay : uint8
{
	Day      UMETA(DisplayName = "Day"),
	Twilight UMETA(DisplayName = "Twilight"),
	Night    UMETA(DisplayName = "Night (lights)")
};

/**
 * FCricketGroundDimensions — the playing area. The pitch axis is the "straight"
 * line (striker -> bowler); the boundary ellipse is centred on the ground centre.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketGroundDimensions
{
	GENERATED_BODY()

	/** Ground centre (m) — the boundary ellipse centre (≈ the pitch centre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground") FVector CenterM = FVector::ZeroVector;

	/** Unit "straight"/down-the-ground axis (striker -> bowler). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground") FVector PitchAxis = FVector(1, 0, 0);

	/** Straight boundary distance from the centre, along the pitch axis (m). ~70–82. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "40.0")) double StraightBoundaryM = 75.0;

	/** Square boundary distance from the centre, perpendicular (m). ~60–75. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "40.0")) double SquareBoundaryM = 68.0;

	/** Rope height (m). A ball crossing above this on the full is a six. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground") double RopeHeightM = 0.15;

	/** Pitch length (m). MCC = 20.12 (stump to stump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground") double PitchLengthM = 20.12;

	/** Off-side direction for a right-hander (perpendicular to the pitch axis). */
	FVector SideAxis() const
	{
		return FVector::CrossProduct(FVector(0, 0, 1), PitchAxis).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	}
	FVector StrikerStumpsM() const { return CenterM - PitchAxis.GetSafeNormal() * (PitchLengthM * 0.5); }
	FVector BowlerStumpsM()  const { return CenterM + PitchAxis.GetSafeNormal() * (PitchLengthM * 0.5); }
};

/** A fielding position's parametric definition: angle from straight (toward off) + depth. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketFieldPositionDef
{
	GENERATED_BODY()

	/** Angle (deg) around the striker, 0 = straight (down the ground), + toward the off side. */
	UPROPERTY(BlueprintReadOnly, Category = "Field") double AngleDeg = 0.0;
	/** Depth as a fraction of the boundary radius in that direction (0 close .. ~0.9 deep). */
	UPROPERTY(BlueprintReadOnly, Category = "Field") double DepthFrac = 0.5;
};

/**
 * FCricketVenueEnvironment — the conditions the match is played in. The atmosphere
 * is the SAME FCricketEnvironment the ball aerodynamics already consume, so wind /
 * humidity / pressure are a real, wired integration (not a stub); day/night adds
 * the floodlight + visibility hook on top.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketVenueEnvironment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment") ECricketTimeOfDay TimeOfDay = ECricketTimeOfDay::Day;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment") bool bFloodlightsOn = false;
	/** Temperature / humidity / pressure / WIND — fed straight to the ball integrator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment") FCricketEnvironment Atmosphere;
};

/**
 * FCricketFieldPlacement — a named arrangement of fielding positions (a "field").
 * The unit a future captain-AI / field-setting system will choose and mutate.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketFieldPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") FString Name = TEXT("Default");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") TArray<ECricketFieldPosition> Positions;
};
